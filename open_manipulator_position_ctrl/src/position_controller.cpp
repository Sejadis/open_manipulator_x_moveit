/*******************************************************************************
* Copyright 2016 ROBOTIS CO., LTD.
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

/* Authors: Taehoon Lim (Darby) */

#include "open_manipulator_position_ctrl/position_controller.h"

using namespace open_manipulator_position_ctrl;

PositionController::PositionController()
    :nh_priv_("~"),
     is_moving_(false),
     move_time_(0.0),
     all_time_steps_(0.0),
     step_cnt_(0),
     moveit_execution_(false),
     gripper_(false)
{
  // Init parameter
  nh_.param("is_debug", is_debug_, is_debug_);

  // ROS Publisher
  goal_joint_position_pub_   = nh_.advertise<sensor_msgs::JointState>("/robotis/open_manipulator/goal_joint_states", 10);

  // ROS Subscriber
  present_joint_position_sub_     = nh_.subscribe("/robotis/open_manipulator/present_joint_states", 10,
                                                    &PositionController::presentJointPositionMsgCallback, this);
  move_group_feedback_sub_        = nh_.subscribe("/move_group/feedback", 10,
                                                    &PositionController::moveGroupActionFeedbackMsgCallback, this);
  display_planned_path_sub_       = nh_.subscribe("/move_group/display_planned_path", 10,
                                                    &PositionController::displayPlannedPathMsgCallback, this);

  gripper_position_sub_           = nh_.subscribe("/robotis/open_manipulator/gripper", 10,
                                                    &PositionController::gripperPositionMsgCallback, this);

  // Init target name
  ROS_ASSERT(initPositionController());
}

PositionController::~PositionController()
{
  ROS_ASSERT(shutdownPositionController());
}

bool PositionController::initPositionController(void)
{
  joint_id_["joint1"] = 1;
  joint_id_["joint2"] = 2;
  joint_id_["joint3"] = 3;
  joint_id_["joint4"] = 4;

  present_joint_position_   = Eigen::VectorXd::Zero(MAX_JOINT_NUM+MAX_GRIPPER_NUM);
  goal_joint_position_      = Eigen::VectorXd::Zero(MAX_JOINT_NUM);
  goal_gripper_position_    = Eigen::VectorXd::Zero(MAX_GRIPPER_NUM);

  motionPlanningTool_ = new motion_planning_tool::MotionPlanningTool();

  motionPlanningTool_->init("robot_description");

  ROS_INFO("open_manipulator_position_controller : Init OK!");
  return true;
}

bool PositionController::shutdownPositionController(void)
{
  ros::shutdown();
  return true;
}

void PositionController::gripOn(void)
{
  Eigen::VectorXd initial_position = Eigen::VectorXd::Zero(MAX_GRIPPER_NUM);
  initial_position(0) = present_joint_position_(4);

  goal_gripper_position_(0) = -75.0 * DEGREE2RADIAN;
  Eigen::VectorXd target_position = goal_gripper_position_;

  move_time_ = 2.0;
  calculateGripperGoalTrajectory(initial_position, target_position);
}

void PositionController::gripOff(void)
{
  Eigen::VectorXd initial_position = Eigen::VectorXd::Zero(MAX_GRIPPER_NUM);
  initial_position(0) = present_joint_position_(4);

  goal_gripper_position_(0) = 0.0 * DEGREE2RADIAN;
  Eigen::VectorXd target_position = goal_gripper_position_;

  move_time_ = 2.0;
  calculateGripperGoalTrajectory(initial_position, target_position);
}

void PositionController::calculateGripperGoalTrajectory(Eigen::VectorXd initial_position, Eigen::VectorXd target_position)
{
  /* set movement time */
  all_time_steps_ = int(floor((move_time_ / ITERATION_TIME) + 1.0));
  move_time_ = double(all_time_steps_ - 1) * ITERATION_TIME;

  goal_gripper_trajectory_.resize(all_time_steps_, MAX_GRIPPER_NUM);

  /* calculate gripper trajectory */
  for (int index = 0; index < MAX_GRIPPER_NUM; index++)
  {
    double init_position_value = initial_position(index);
    double target_position_value = target_position(index);

    Eigen::MatrixXd trajectory =
        robotis_framework::calcMinimumJerkTra(init_position_value, 0.0, 0.0,
                                              target_position_value, 0.0, 0.0,
                                              ITERATION_TIME, move_time_);

    // Block of size (p,q), starting at (i,j)
    // block(i,j,p,q)
    goal_gripper_trajectory_.block(0, index, all_time_steps_, 1) = trajectory;
  }

  step_cnt_   = 0;
  is_moving_  = true;
  gripper_    = true;

  ROS_INFO("Start Gripper Trajectory");
}

void PositionController::moveGroupActionFeedbackMsgCallback(const moveit_msgs::MoveGroupActionFeedback::ConstPtr &msg)
{
  if (is_moving_ == false && msg->feedback.state == "MONITOR")
  {
    moveit_execution_ = true;
  }
}

void PositionController::displayPlannedPathMsgCallback(const moveit_msgs::DisplayTrajectory::ConstPtr &msg)
{
  /*
  # The model id for which this path has been generated

  string model_id

  # The representation of the path contains position values for all the joints that are moving along the path;
  # a sequence of trajectories may be specified

  RobotTrajectory[] trajectory

  # The robot state is used to obtain positions for all/some of the joints of the robot.
  # It is used by the path display node to determine the positions of the joints that are not specified in the joint path message above.
  # If the robot state message contains joint position information for joints that are also mentioned in the joint path message,
  # the positions in the joint path message will overwrite the positions specified in the robot state message.

  RobotState trajectory_start
  */

  motionPlanningTool_->moveit_msg_ = *msg;

  trajectory_generate_thread_ = new boost::thread(boost::bind(&PositionController::moveItTragectoryGenerateThread, this));
  delete trajectory_generate_thread_;
}

void PositionController::moveItTragectoryGenerateThread()
{
  std::vector<double> via_time;

  for (int _tra_index = 0; _tra_index < motionPlanningTool_->moveit_msg_.trajectory.size(); _tra_index++)
  {
    motionPlanningTool_->points_ = motionPlanningTool_->moveit_msg_.trajectory[_tra_index].joint_trajectory.points.size();

    motionPlanningTool_->display_planned_path_positions_.resize(motionPlanningTool_->points_, MAX_JOINT_NUM);
    motionPlanningTool_->display_planned_path_velocities_.resize(motionPlanningTool_->points_, MAX_JOINT_NUM);
    motionPlanningTool_->display_planned_path_accelerations_.resize(motionPlanningTool_->points_, MAX_JOINT_NUM);

    for (int _point_index = 0; _point_index < motionPlanningTool_->points_; _point_index++)
    {
      for(int _name_index = 0; _name_index < MAX_JOINT_NUM; _name_index++)
      {
        motionPlanningTool_->display_planned_path_positions_.coeffRef(_point_index, _name_index) = goal_joint_position_(_name_index);
      }
    }

    for (int _point_index = 0; _point_index < motionPlanningTool_->moveit_msg_.trajectory[_tra_index].joint_trajectory.points.size(); _point_index++)
    {
      motionPlanningTool_->time_from_start_ = motionPlanningTool_->moveit_msg_.trajectory[_tra_index].joint_trajectory.points[_point_index].time_from_start;
      via_time.push_back(motionPlanningTool_->time_from_start_.toSec());

      for (int _joint_index = 0; _joint_index < motionPlanningTool_->moveit_msg_.trajectory[_tra_index].joint_trajectory.joint_names.size(); _joint_index++)
      {
        std::string _joint_name 	  = motionPlanningTool_->moveit_msg_.trajectory[_tra_index].joint_trajectory.joint_names[_joint_index];

        double _joint_position 		  = motionPlanningTool_->moveit_msg_.trajectory[_tra_index].joint_trajectory.points[_point_index].positions	   [_joint_index];
        double _joint_velocity 	   	= motionPlanningTool_->moveit_msg_.trajectory[_tra_index].joint_trajectory.points[_point_index].velocities	 [_joint_index];
        double _joint_acceleration 	= motionPlanningTool_->moveit_msg_.trajectory[_tra_index].joint_trajectory.points[_point_index].accelerations[_joint_index];

        motionPlanningTool_->display_planned_path_positions_.coeffRef     (_point_index , joint_id_[_joint_name]-1) = _joint_position;
        motionPlanningTool_->display_planned_path_velocities_.coeffRef	  (_point_index , joint_id_[_joint_name]-1) = _joint_velocity;
        motionPlanningTool_->display_planned_path_accelerations_.coeffRef (_point_index , joint_id_[_joint_name]-1) = _joint_acceleration;
      }
    }
  }

  move_time_ = motionPlanningTool_->time_from_start_.toSec();

  all_time_steps_ = motionPlanningTool_->points_;

  ros::Duration seconds(0.5);
  seconds.sleep();


  goal_joint_trajectory_ = motionPlanningTool_->display_planned_path_positions_;
  ROS_INFO("Get Joint Trajectory");

  if (moveit_execution_ == true)
  {
    is_moving_    = true;
    step_cnt_     = 0;

    moveit_execution_ = false;

    ROS_INFO("Send Motion Trajectory");
  }
}

void PositionController::presentJointPositionMsgCallback(const sensor_msgs::JointState::ConstPtr &msg)
{
  for (int index = 0; index < MAX_JOINT_NUM + MAX_GRIPPER_NUM; index++)
  {
    present_joint_position_(index) = msg->position.at(index);
  }
}

void PositionController::gripperPositionMsgCallback(const std_msgs::String::ConstPtr &msg)
{
  if (msg->data == "grip_on")
  {
    gripOn();
  }
  else if (msg->data == "grip_off")
  {
    gripOff();
  }
  else
  {
    ROS_ERROR("If you want to grip or release something, publish 'grip_on' or 'grip_off'");
  }
}

void PositionController::process(void)
{
  // Get Joint & Gripper State
  // present_joint_position, present_gripper_position

  if (is_moving_)
  {
    if (gripper_)
    {
      for (int index = 0; index < MAX_GRIPPER_NUM; index++)
      {
        goal_gripper_position_(index) = goal_gripper_trajectory_(step_cnt_, index);
      }
    }
    else
    {
      for (int index = 0; index < MAX_JOINT_NUM; index++)
      {
        goal_joint_position_(index) = goal_joint_trajectory_(step_cnt_, index);
      }
    }

    step_cnt_++;
  }

  sensor_msgs::JointState send_to_joint_position;

  for (std::map<std::string, uint8_t>::iterator state_iter = joint_id_.begin();
       state_iter != joint_id_.end(); state_iter++)
  {
    std::string joint_name = state_iter->first;
    send_to_joint_position.name.push_back(joint_name);
    send_to_joint_position.position.push_back(goal_joint_position_(joint_id_[joint_name]-1));
  }

  std::string gripper_name = "grip_joint";
  send_to_joint_position.name.push_back(gripper_name);
  send_to_joint_position.position.push_back(goal_gripper_position_(0));

  if (is_moving_)
  {
    goal_joint_position_pub_.publish(send_to_joint_position);
  }

  if (is_moving_)
  {
    if (step_cnt_ >= all_time_steps_)
    {
      is_moving_ = false;
      step_cnt_  = 0;
      gripper_   = false;

      ROS_INFO("End Trajectory");
    }
  }
}

int main(int argc, char **argv)
{
  // Init ROS node
  ros::init(argc, argv, "open_manipulator_position_controller");
  PositionController position_controller;
  ros::Rate loop_rate(ITERATION_FREQUENCY);

  while (ros::ok())
  {
    position_controller.process();
    ros::spinOnce();
    loop_rate.sleep();
  }

  return 0;
}