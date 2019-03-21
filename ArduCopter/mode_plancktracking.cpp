#include "Copter.h"

bool Copter::ModePlanckTracking::init(bool ignore_checks){
    //If the copter is currently in-flight, entry into this mode indicates
    //that the user would like to return to the tag tracking, so run RTB
    if(!copter.ap.land_complete)
    {
        //Return home rate is either WPNAV_SPEED or RTL_SPEED, if specified
        float rate_xy_cms = g.rtl_speed_cms != 0 ? g.rtl_speed_cms : copter.wp_nav->get_speed_xy();

        copter.planck_interface.request_rtb(
          (float)copter.g.rtl_altitude/100.,
          copter.pos_control->get_speed_up()/100.,
          copter.pos_control->get_speed_down()/100.,
          rate_xy_cms/100.);
    }

    //Otherwise do nothing, as we are on the ground waiting for a takeoff
    //command

    return Copter::ModeGuided::init(ignore_checks);
}

void Copter::ModePlanckTracking::run() {

    //If there is new command data, send it to Guided
    if(copter.planck_interface.new_command_available()) {
        switch(copter.planck_interface.get_cmd_type()) {
          //Set guided mode attitude/velocity commands
          case copter.planck_interface.ACCEL:
          {
              Vector3f accel_cmss;
              float yaw_cd;
              float vz_cms;
              bool is_yaw_rate;

              bool good_cmd = copter.planck_interface.get_accel_yaw_zrate_cmd(
                  accel_cmss, yaw_cd, vz_cms, is_yaw_rate
              );

              if(!good_cmd) {
                  accel_cmss.x = accel_cmss.y = yaw_cd = vz_cms = 0;
                  is_yaw_rate = true;
              }

              //Turn accel into lean angles
              float roll_cd, pitch_cd;
              copter.pos_control->accel_to_lean_angles(
                accel_cmss.x,
                accel_cmss.y,
                roll_cd,
                pitch_cd);

              //Convert this to quaternions, yaw rates
              Quaternion q;
              q.from_euler(ToRad(roll_cd/100.), ToRad(pitch_cd/100.), ToRad(yaw_cd/100.));
              float yaw_rate_rads = ToRad(yaw_cd / 100.);

              //Update the GUIDED mode controller
              Copter::ModeGuided::set_angle(q,vz_cms,is_yaw_rate,yaw_rate_rads);
              break;
          }

          case copter.planck_interface.ATTITUDE:
          {
              Vector3f att_cd;
              float vz_cms;
              bool is_yaw_rate;

              bool good_cmd = copter.planck_interface.get_attitude_zrate_cmd(
                  att_cd, vz_cms, is_yaw_rate
              );

              if(!good_cmd) {
                  att_cd.x = att_cd.y = att_cd.z = vz_cms = 0;
                  is_yaw_rate = true;
              }

              //Convert this to quaternions, yaw rates
              Quaternion q;
              q.from_euler(ToRad(att_cd.x/100.), ToRad(att_cd.y/100.), ToRad(att_cd.z/100.));
              float yaw_rate_rads = ToRad(att_cd.z / 100.);

              //Update the GUIDED mode controller
              Copter::ModeGuided::set_angle(q,vz_cms,is_yaw_rate,yaw_rate_rads);
              break;
          }

          case copter.planck_interface.VELOCITY:
          {
              Vector3f vel_cmd;

              bool good_cmd = copter.planck_interface.get_velocity_cmd(vel_cmd);

              if(!good_cmd) {
                  vel_cmd.x = vel_cmd.y = vel_cmd.z = 0;
              }

              Copter::ModeGuided::set_velocity(vel_cmd);
              break;
          }

          case copter.planck_interface.POSITION:
          {
              Location loc_cmd;
              copter.planck_interface.get_position_cmd(loc_cmd);
              Copter::ModeGuided::set_destination(loc_cmd);
              break;
          }

          case copter.planck_interface.POSVEL:
          {
              Location loc_cmd;
              Vector3f vel_cmd;

              bool good_cmd = copter.planck_interface.get_posvel_cmd(
                loc_cmd,
                vel_cmd);

              //Set a zero velocity if this is a bad command
              if(!good_cmd)
              {
                  vel_cmd.x = vel_cmd.y = vel_cmd.z = 0;
                  Copter::ModeGuided::set_velocity(vel_cmd);
              }
              else
              {
                  Copter::ModeGuided::set_destination_posvel(
                    copter.pv_location_to_vector(loc_cmd),
                    vel_cmd);
              }
              break;
          }

          default:
            break;
      }
    }

    //Run the guided mode controller
    Copter::ModeGuided::run();
}

bool Copter::ModePlanckTracking::do_user_takeoff_start(float final_alt_above_home)
{
    // Check if planck is ready
    if(!copter.planck_interface.ready_for_takeoff())
      return false;

    // Tell planck to start commanding
    copter.planck_interface.request_takeoff(final_alt_above_home/100.);

    // initialise yaw
    auto_yaw.set_mode(AUTO_YAW_HOLD);

    // clear i term when we're taking off
    set_throttle_takeoff();

    return true;
}

//Allow arming if planck is ready for takeooff and this is a GCS command
bool Copter::ModePlanckTracking::allows_arming(bool from_gcs) const
{
    if(!from_gcs) return false;
    if(!copter.planck_interface.ready_for_takeoff())
    {
        if(!copter.planck_interface.get_tag_tracking_state())
        {
            copter.gcs().send_text(MAV_SEVERITY_CRITICAL,
              "Arm: Planck not tracking tag");
        }
        else
        {
            copter.gcs().send_text(MAV_SEVERITY_CRITICAL,
              "Arm: Planck not ready for takeoff");
        }

        return false;
    }

    if((copter.arming.checks_to_perform & AP_Arming::ARMING_CHECK_ALL) ||
       (copter.arming.checks_to_perform & AP_Arming::ARMING_CHECK_PLANCK_GPS))
    {
        if(!copter.planck_interface.get_commbox_state())
        {
            copter.gcs().send_text(MAV_SEVERITY_CRITICAL,
              "Arm: Planck not tracking Commbox GPS");
            return false;
        }
    }

    return true;
}
