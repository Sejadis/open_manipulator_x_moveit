/*******************************************************************************
* Copyright 2019 ROBOTIS CO., LTD.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

/* Authors: Ryan Shim */

#include "open_manipulator_x_teleop/open_manipulator_x_teleop_joystick.hpp"

namespace open_manipulator_x_teleop_joystick
{
OpenManipulatorXTeleopJoystick::OpenManipulatorXTeleopJoystick()
: Node("open_manipulator_x_teleop_joystick")
{
  /*****************************************************************************
  ** Initialise joint angle and kinematic position size 
  *****************************************************************************/
  present_joint_angle_.resize(NUM_OF_JOINT);
  present_kinematic_position_.resize(3);

  /*****************************************************************************
  ** Initialise Subscribers
  *****************************************************************************/
  joint_states_sub_ = this->create_subscription<sensor_msgs::msg::JointState>(
    "open_manipulator_x/joint_states", 10, std::bind(&OpenManipulatorXTeleopJoystick::joint_states_callback, this, std::placeholders::_1));
  kinematics_pose_sub_ = this->create_subscription<open_manipulator_msgs::msg::KinematicsPose>(
    "open_manipulator_x/kinematics_pose", 10, std::bind(&OpenManipulatorXTeleopJoystick::kinematics_pose_callback, this, std::placeholders::_1));
  joy_command_sub_ = this->create_subscription<sensor_msgs::msg::Joy>(
    "joy", 10, std::bind(&OpenManipulatorXTeleopJoystick::joy_callback, this, std::placeholders::_1));

  /*****************************************************************************
  ** Initialise Clients
  *****************************************************************************/
  goal_joint_space_path_client_ = this->create_client<open_manipulator_msgs::srv::SetJointPosition>("open_manipulator_x/goal_joint_space_path");
  goal_tool_control_client_ = this->create_client<open_manipulator_msgs::srv::SetJointPosition>("open_manipulator_x/goal_tool_control");
  goal_task_space_path_from_present_position_only_client_ = this->create_client<open_manipulator_msgs::srv::SetKinematicsPose>("open_manipulator_x/goal_task_space_path_from_present_position_only");

  RCLCPP_INFO(this->get_logger(), "OpenManipulator Initialised");
}

OpenManipulatorXTeleopJoystick::~OpenManipulatorXTeleopJoystick() {}

/*****************************************************************************
** Callback Functions
*****************************************************************************/
void OpenManipulatorXTeleopJoystick::joint_states_callback(const sensor_msgs::msg::JointState::SharedPtr msg)
{
  std::vector<double> temp_angle;
  temp_angle.resize(NUM_OF_JOINT);
  for(std::vector<int>::size_type i = 0; i < msg->name.size(); i ++)
  {
    if(!msg->name.at(i).compare("joint1"))  temp_angle.at(0) = (msg->position.at(i));
    else if(!msg->name.at(i).compare("joint2"))  temp_angle.at(1) = (msg->position.at(i));
    else if(!msg->name.at(i).compare("joint3"))  temp_angle.at(2) = (msg->position.at(i));
    else if(!msg->name.at(i).compare("joint4"))  temp_angle.at(3) = (msg->position.at(i));
  }
  present_joint_angle_ = temp_angle;
}

void OpenManipulatorXTeleopJoystick::kinematics_pose_callback(const open_manipulator_msgs::msg::KinematicsPose::SharedPtr msg)
{
  std::vector<double> temp_position;
  temp_position.push_back(msg->pose.position.x);
  temp_position.push_back(msg->pose.position.y);
  temp_position.push_back(msg->pose.position.z);
  present_kinematic_position_ = temp_position;
}

void OpenManipulatorXTeleopJoystick::joy_callback(const sensor_msgs::msg::Joy::SharedPtr msg)
{
  if(msg->axes.at(1) >= 0.9) set_goal("x+");
  else if(msg->axes.at(1) <= -0.9) set_goal("x-");
  else if(msg->axes.at(0) >=  0.9) set_goal("y+");
  else if(msg->axes.at(0) <= -0.9) set_goal("y-");
  else if(msg->buttons.at(3) == 1) set_goal("z+");
  else if(msg->buttons.at(0) == 1) set_goal("z-");
  else if(msg->buttons.at(5) == 1) set_goal("home");
  else if(msg->buttons.at(4) == 1) set_goal("init");

  if(msg->buttons.at(2) == 1) set_goal("gripper close");
  else if(msg->buttons.at(1) == 1) set_goal("gripper open");
}

/*****************************************************************************
** Callback Functions and Relevant Functions
*****************************************************************************/
void OpenManipulatorXTeleopJoystick::set_goal(const char* str)
{
  std::vector<double> goalPose;  goalPose.resize(3,0);
  std::vector<double> goalJoint; goalJoint.resize(4,0);

  if(str == "x+")
  {
    printf("increase(++) x axis in cartesian space\n");
    goalPose.at(0) = DELTA;
    set_task_space_path_from_present_position_only(goalPose, PATH_TIME);
  }
  else if(str == "x-")
  {
    printf("decrease(--) x axis in cartesian space\n");
    goalPose.at(0) = -DELTA;
    set_task_space_path_from_present_position_only(goalPose, PATH_TIME);
  }
  else if(str == "y+")
  {
    printf("increase(++) y axis in cartesian space\n");
    goalPose.at(1) = DELTA;
    set_task_space_path_from_present_position_only(goalPose, PATH_TIME);
  }
  else if(str == "y-")
  {
    printf("decrease(--) y axis in cartesian space\n");
    goalPose.at(1) = -DELTA;
    set_task_space_path_from_present_position_only(goalPose, PATH_TIME);
  }
  else if(str == "z+")
  {
    printf("increase(++) z axis in cartesian space\n");
    goalPose.at(2) = DELTA;
    set_task_space_path_from_present_position_only(goalPose, PATH_TIME);
  }
  else if(str == "z-")
  {
    printf("decrease(--) z axis in cartesian space\n");
    goalPose.at(2) = -DELTA;
    set_task_space_path_from_present_position_only(goalPose, PATH_TIME);
  }
  else if(str == "gripper open")
  {
    printf("open gripper\n");
    std::vector<double> joint_angle;

    joint_angle.push_back(0.01);
    set_tool_control(joint_angle);
  }
  else if(str == "gripper close")
  {
    printf("close gripper\n");
    std::vector<double> joint_angle;
    joint_angle.push_back(-0.01);
    set_tool_control(joint_angle);
  }
  else if(str == "home")
  {
    printf("home pose\n");
    std::vector<std::string> joint_name;
    std::vector<double> joint_angle;
    double path_time = 2.0;

    joint_name.push_back("joint1"); joint_angle.push_back(0.0);
    joint_name.push_back("joint2"); joint_angle.push_back(-1.05);
    joint_name.push_back("joint3"); joint_angle.push_back(0.35);
    joint_name.push_back("joint4"); joint_angle.push_back(0.70);
    set_joint_space_path(joint_name, joint_angle, path_time);
  }
  else if(str == "init")
  {
    printf("init pose\n");

    std::vector<std::string> joint_name;
    std::vector<double> joint_angle;
    double path_time = 2.0;
    joint_name.push_back("joint1"); joint_angle.push_back(0.0);
    joint_name.push_back("joint2"); joint_angle.push_back(0.0);
    joint_name.push_back("joint3"); joint_angle.push_back(0.0);
    joint_name.push_back("joint4"); joint_angle.push_back(0.0);
    set_joint_space_path(joint_name, joint_angle, path_time);
  }
}

bool OpenManipulatorXTeleopJoystick::set_joint_space_path(std::vector<std::string> joint_name, std::vector<double> joint_angle, double path_time)
{
  auto request = std::make_shared<open_manipulator_msgs::srv::SetJointPosition::Request>();
  request->joint_position.joint_name = joint_name;
  request->joint_position.position = joint_angle;
  request->path_time = path_time;
  
  using ServiceResponseFuture = rclcpp::Client<open_manipulator_msgs::srv::SetJointPosition>::SharedFuture;
  auto response_received_callback = [this](ServiceResponseFuture future) {
      auto result = future.get();
      return result->is_planned;
  };
  auto future_result = goal_joint_space_path_client_->async_send_request(request, response_received_callback);

  return false;
}

bool OpenManipulatorXTeleopJoystick::set_tool_control(std::vector<double> joint_angle)
{
  auto request = std::make_shared<open_manipulator_msgs::srv::SetJointPosition::Request>();
  request->joint_position.joint_name.push_back("gripper");
  request->joint_position.position = joint_angle;

  using ServiceResponseFuture = rclcpp::Client<open_manipulator_msgs::srv::SetJointPosition>::SharedFuture;
  auto response_received_callback = [this](ServiceResponseFuture future) {
      auto result = future.get();
      return result->is_planned;
  };
  auto future_result = goal_tool_control_client_->async_send_request(request, response_received_callback);

  return false;
}

bool OpenManipulatorXTeleopJoystick::set_task_space_path_from_present_position_only(std::vector<double> kinematics_pose, double path_time)
{
  auto request = std::make_shared<open_manipulator_msgs::srv::SetKinematicsPose::Request>();
  request->planning_group = "gripper";
  request->kinematics_pose.pose.position.x = kinematics_pose.at(0);
  request->kinematics_pose.pose.position.y = kinematics_pose.at(1);
  request->kinematics_pose.pose.position.z = kinematics_pose.at(2);
  request->path_time = path_time;

  using ServiceResponseFuture = rclcpp::Client<open_manipulator_msgs::srv::SetKinematicsPose>::SharedFuture;
  auto response_received_callback = [this](ServiceResponseFuture future) {
      auto result = future.get();
      return result->is_planned;
  };
  auto future_result = goal_task_space_path_from_present_position_only_client_->async_send_request(request, response_received_callback);

  return false;
}
}  // namespace open_manipulator_x_teleop_joystick

/*****************************************************************************
** Main
*****************************************************************************/
int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);

  rclcpp::spin(std::make_shared<open_manipulator_x_teleop_joystick::OpenManipulatorXTeleopJoystick>());

  rclcpp::shutdown();

  return 0;
}