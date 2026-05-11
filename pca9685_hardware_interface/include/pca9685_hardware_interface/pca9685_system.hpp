#ifndef PCA9685_HARDWARE_INTERFACE__PCA9685_SYSTEM_HPP_
#define PCA9685_HARDWARE_INTERFACE__PCA9685_SYSTEM_HPP_

#include <memory>
#include <string>
#include <vector>
#include <gpiod.h>
#include <algorithm>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/clock.hpp"
#include "rclcpp/duration.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp/time.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "pca9685_hardware_interface/visibility_control.h"
#include <pca9685_hardware_interface/pca9685_comm.h>

namespace pca9685_hardware_interface
{
class Pca9685SystemHardware : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(Pca9685SystemHardware);

  PCA9685_HARDWARE_INTERFACE_PUBLIC
  hardware_interface::CallbackReturn on_init(const hardware_interface::HardwareComponentInterfaceParams & params) override;

  PCA9685_HARDWARE_INTERFACE_PUBLIC
  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  PCA9685_HARDWARE_INTERFACE_PUBLIC
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  PCA9685_HARDWARE_INTERFACE_PUBLIC
  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  PCA9685_HARDWARE_INTERFACE_PUBLIC
  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  PCA9685_HARDWARE_INTERFACE_PUBLIC
  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  PCA9685_HARDWARE_INTERFACE_PUBLIC
  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  ~Pca9685SystemHardware();

private:
  std::vector<double> hw_commands_;
  std::vector<double> hw_position_;
  std::vector<double> hw_velocity_;
  std::vector<int>    last_encoder_ticks_;
  std::vector<int>    encoder_ticks_;
  std::vector<int>    motor_direction_;
  rclcpp::Time last_time_;
  int read_right_encoder();
  int read_left_encoder();
  double ticks_per_rev_;
  double wheel_radius_;
  double wheel_circumference_;
  double max_velocity_;
  PiPCA9685::PCA9685 pca;
  gpiod_chip * chip_;

  std::vector<gpiod_line*> motor_in1_;
  std::vector<gpiod_line*> motor_in2_;
  std::vector<int> dir_pins_;
  bool request_output_line(gpiod_line* line, const std::string& name);
};

}  // namespace pca9685_hardware_interface

#endif  // PCA9685_HARDWARE_INTERFACE__PCA9685_SYSTEM_HPP_
