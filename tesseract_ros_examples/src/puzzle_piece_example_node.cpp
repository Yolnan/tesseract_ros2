/**
 * @file puzzle_piece_example_node.cpp
 * @brief Puzzle piece example node
 *
 * @author Levi Armstrong
 * @date July 22, 2019
 * @version TODO
 * @bug No known bugs
 *
 * @copyright Copyright (c) 2017, Southwest Research Institute
 *
 * @par License
 * Software License Agreement (Apache License)
 * @par
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * @par
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <thread>

#include <tesseract_ros_examples/puzzle_piece_example.h>

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<rclcpp::Node>("puzzle_piece_example_node");

  bool plotting = node->declare_parameter("plotting", true);
  bool rviz = node->declare_parameter("rviz", true);

  tesseract_ros_examples::PuzzlePieceExample example(node, plotting, rviz);

  std::thread spinner{
    [node]()
    {
      rclcpp::spin(node);
    }
  };

  example.run();

  RCLCPP_INFO(node->get_logger(), "Example completed, waiting for ROS shutdown...");
  spinner.join();
  RCLCPP_INFO(node->get_logger(), "Shutdown!");

  return 0;
}
