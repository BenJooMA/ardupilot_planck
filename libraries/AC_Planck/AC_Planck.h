#pragma once

#include <AP_Common/AP_Common.h>
#include <GCS_MAVLink/GCS_MAVLink.h>
#include <AP_Common/Location.h>
#include <AP_AHRS/AP_AHRS.h>
#include <AP_InertialNav/AP_InertialNav.h>
#include <AP_GPS/AP_GPS.h>

//Defines the interface to Planck's control software

#define ACK_WAIT_TIME_MS 500

class AC_Planck {

public:

  enum cmd_type {
    NONE=0,
    ACCEL,
    ATTITUDE,
    VELOCITY,
    POSITION,
    POSVEL
  };

  AC_Planck(void) {}

  ~AC_Planck(void) {}

  //Handle incoming messages
  void handle_planck_mavlink_msg(const mavlink_channel_t &chan, const mavlink_message_t *mav_msg, AP_AHRS &ahrs);

  //Requesters to be sent to planck
  void request_takeoff(const float alt);
  void request_alt_change(const float alt, const float rate_up_cms, const float rate_down_cms);
  void request_rtb(const float alt, const float rate_up, const float rate_down, const float rate_xy);
  void request_land(const float descent_rate);
  void request_move_target(const Vector3f offset_cmd_NED, const bool is_rate, const float rate_up_cms, const float rate_down_cms);
  void stop_commanding(void);

  //planck status getters
  bool ready_for_takeoff(void) { return _is_status_ok() && _status.takeoff_ready; };
  bool ready_for_land(void) { return _is_status_ok() && _status.land_ready; };
  bool is_takeoff_complete() { return _status.takeoff_complete; };
  bool get_commbox_state() { return _status.commbox_ok && _status.commbox_gps_ok && _status.tracking_commbox_gps; };
  bool get_tag_tracking_state() { return _status.tracking_tag; };

  //oneshot _was_at_location
  bool at_location() { bool tmp(_was_at_location); _was_at_location = false; return tmp; };

  //command getters
  cmd_type get_cmd_type(void) { return _cmd.type; }
  bool new_command_available() { return _cmd.is_new; };

  //Get an accel, yaw, z_rate command
  bool get_accel_yaw_zrate_cmd(Vector3f &accel_cmss, float &yaw_cd, float &vz_cms, bool &is_yaw_rate);

  //Get an attitude command
  bool get_attitude_zrate_cmd(Vector3f &att_cd, float &vz_cms, bool &is_yaw_rate);

  //Get a velocity, yaw command
  bool get_velocity_cmd(Vector3f &vel_cms);

  //Get a position command
  bool get_position_cmd(Location &loc);

  //Get a position, velocity, yaw command
  bool get_posvel_cmd(Location &loc, Vector3f &vel_cms, float &yaw_cd, bool &is_yaw_rate);

  // handle ack/nack message from ACE
  void handle_planck_ack(const mavlink_message_t &msg);

  uint32_t mux_rates(float rate_up,  float rate_down);

  //Returns true if the last command req was actively NACKd or timed out
  bool was_last_request_rejected();

  //If waiting for an ack, it returns the the last cmd req set, otherwise returns -1
  int waiting_for_ack();

  uint16_t get_last_cmd_req_id()  { return _cmd_req_info.last_cmd_req_id; };

  uint32_t get_last_cmd_req_t_ms() { return _cmd_req_info.last_cmd_req_t_ms; };

private:

  struct
  {
    Location pos;
    Vector3f vel_cms = Vector3f(0,0,0);
    Vector3f accel_cmss = Vector3f(0,0,0);
    Vector3f att_cd = Vector3f(0,0,0);
    bool is_yaw_rate = true;
    uint32_t timestamp_ms = 0;
    bool is_new = false;
    cmd_type type = NONE;
  }_cmd;

  struct
  {
    bool takeoff_ready = false;
    bool land_ready = false;
    bool commbox_ok = false;
    bool commbox_gps_ok = false;
    bool tracking_tag = false;
    bool tracking_commbox_gps = false;
    bool takeoff_complete = false;
    bool at_location = false;
  }_status;

  enum planck_ack_status
  {
    NOT_WAITING,
    PLANCK_ACK,
    PLANCK_NACK,
    PLANCK_WAITING_FOR_ACK
  };

  struct
  {
    uint32_t last_cmd_req_t_ms = 0;
    int16_t last_cmd_req_id = -1;
    planck_ack_status ack_status = NOT_WAITING;
  } _cmd_req_info;


  mavlink_channel_t _chan = MAVLINK_COMM_1;

  bool _was_at_location = false; //For debouncing at-location

  bool _is_status_ok(void) { return ((AP_HAL::millis() - _status.timestamp_ms) < ACK_WAIT_TIME_MS); }

  void _sent_cmd_req(uint16_t id) {
    _cmd_req_info.last_cmd_req_t_ms = AP_HAL::millis();
    _cmd_req_info.ack_status = PLANCK_WAITING_FOR_ACK;
    _cmd_req_info.last_cmd_req_id = id;
  };

  void _set_ack_status(planck_ack_status ack_status){ _cmd_req_info.ack_status = ack_status; };

};
