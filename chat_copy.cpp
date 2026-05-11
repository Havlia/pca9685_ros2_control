#include "pca9685_hardware_interface/pca9685_system.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <gpiod.h>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

namespace pca9685_hardware_interface
{

hardware_interface::CallbackReturn Pca9685SystemHardware::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) !=
      hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }

  size_t n = info_.joints.size();

  hw_commands_.assign(n, 0.0);
  hw_position_.assign(n, 0.0);
  hw_velocity_.assign(n, 0.0);
  last_encoder_ticks_.assign(n, 0);
  encoder_ticks_.assign(n, 0);
  motor_direction_.assign(n, 1);

  RCLCPP_INFO(rclcpp::get_logger("Pca9685SystemHardware"),
              "Initialized PCA9685 hardware with %zu joints", n);

  return hardware_interface::CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface>
Pca9685SystemHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;

  for (size_t i = 0; i < info_.joints.size(); i++)
  {
    state_interfaces.emplace_back(info_.joints[i].name,
                                  hardware_interface::HW_IF_POSITION,
                                  &hw_position_[i]);

    state_interfaces.emplace_back(info_.joints[i].name,
                                  hardware_interface::HW_IF_VELOCITY,
                                  &hw_velocity_[i]);
  }

  return state_interfaces;
}

std::vector<hardware_interface::CommandInterface>
Pca9685SystemHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;

  for (size_t i = 0; i < info_.joints.size(); i++)
  {
    command_interfaces.emplace_back(info_.joints[i].name,
                                    hardware_interface::HW_IF_VELOCITY,
                                    &hw_commands_[i]);
  }

  return command_interfaces;
}

hardware_interface::CallbackReturn Pca9685SystemHardware::on_activate(
  const rclcpp_lifecycle::State &)
{
  std::fill(hw_commands_.begin(), hw_commands_.end(), 0.0);
  std::fill(hw_position_.begin(), hw_position_.end(), 0.0);
  std::fill(hw_velocity_.begin(), hw_velocity_.end(), 0.0);

  RCLCPP_INFO(rclcpp::get_logger("Pca9685SystemHardware"), "Activated");
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn Pca9685SystemHardware::on_deactivate(
  const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(rclcpp::get_logger("Pca9685SystemHardware"), "Deactivated");
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::return_type Pca9685SystemHardware::read(
  const rclcpp::Time &,
  const rclcpp::Duration &)
{
  // ---- READ ENCODERS ----
  int left_ticks  = read_left_encoder();
  int right_ticks = read_right_encoder();

  encoder_ticks_[0] = left_ticks;
  encoder_ticks_[1] = right_ticks;

  // ---- CONVERT TICKS → POSITION + VELOCITY ----
  static const double ticks_per_rev = 1024.0;
  static const double wheel_circumference = 2.0 * M_PI * 0.05; // example: 5cm radius

  for (size_t i = 0; i < 2; i++)
  {
    int delta_ticks = encoder_ticks_[i] - last_encoder_ticks_[i];

    double delta_rot = delta_ticks / ticks_per_rev;
    double delta_pos = delta_rot * wheel_circumference;

    hw_position_[i] += delta_pos;
    hw_velocity_[i] = delta_pos; // crude (better: divide by dt)

    last_encoder_ticks_[i] = encoder_ticks_[i];
  }

  return hardware_interface::return_type::OK;
}

hardware_interface::return_type Pca9685SystemHardware::write(
  const rclcpp::Time &,
  const rclcpp::Duration &)
{
  for (size_t i = 0; i < hw_commands_.size(); i++)
  {
    double cmd = hw_commands_[i];

    // apply motor direction inversion if needed
    cmd *= motor_direction_[i];

    double duty = command_to_duty_cycle(cmd);

    pca.set_pwm_ms(i, duty);
  }

  return hardware_interface::return_type::OK;
}

double Pca9685SystemHardware::command_to_duty_cycle(double command)
{
  double clamped = std::clamp(command, -1.0, 1.0);

  double min_pwm = 0.5;
  double max_pwm = 2.5;

  return ((clamped + 1.0) / 2.0) * (max_pwm - min_pwm) + min_pwm;
}

// -------------------- ENCODERS --------------------

int Pca9685SystemHardware::read_left_encoder()
{
  // TODO: replace with GPIO / interrupt counter
  return 0;
}

int Pca9685SystemHardware::read_right_encoder()
{
  // TODO: replace with GPIO / interrupt counter
  return 0;
}

} // namespace pca9685_hardware_interface

#include "pluginlib/class_list_macros.hpp"

PLUGINLIB_EXPORT_CLASS(
  pca9685_hardware_interface::Pca9685SystemHardware,
  hardware_interface::SystemInterface)