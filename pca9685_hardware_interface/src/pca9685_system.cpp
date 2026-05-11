#include "pca9685_hardware_interface/pca9685_system.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>
#include <gpiod.h>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"



namespace pca9685_hardware_interface
{
hardware_interface::CallbackReturn Pca9685SystemHardware::on_init(const hardware_interface::HardwareComponentInterfaceParams & params)
{

  if (
    hardware_interface::SystemInterface::on_init(params) !=
    hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }

  pca.set_pwm_freq(200);

  ticks_per_rev_ = 700.0;
  wheel_radius_ = 0.025;
  max_velocity_ = 10.0;

  chip_ = gpiod_chip_open("/dev/gpiochip0");

  if (!chip_)
{
  RCLCPP_FATAL(
    rclcpp::get_logger("Pca9685SystemHardware"),
    "Failed to open gpiochip");
  return hardware_interface::CallbackReturn::ERROR;
  }

  if (info_.hardware_parameters.count("ticks_per_rev"))
  {
    ticks_per_rev_ = std::stod(info_.hardware_parameters["ticks_per_rev"]);
  }

  if (info_.hardware_parameters.count("wheel_radius"))
  {
    wheel_radius_ = std::stod(info_.hardware_parameters["wheel_radius"]);
  }
  if (info_.hardware_parameters.count("max_velocity"))
  {
    max_velocity_ = std::stod(info_.hardware_parameters["max_velocity"]);
  }

  if (!info_.hardware_parameters.count("left_inb_pin") ||
      !info_.hardware_parameters.count("left_ina_pin") ||
      !info_.hardware_parameters.count("right_inb_pin") ||
      !info_.hardware_parameters.count("right_ina_pin"))
  {
      return hardware_interface::CallbackReturn::ERROR;
  }
  int left_ina  = std::stoi(info_.hardware_parameters["left_ina_pin"]);
  int left_inb  = std::stoi(info_.hardware_parameters["left_inb_pin"]);
  int right_ina = std::stoi(info_.hardware_parameters["right_ina_pin"]);
  int right_inb = std::stoi(info_.hardware_parameters["right_inb_pin"]);

  std::vector<int> pins = {
  left_ina, left_inb,
  right_ina, right_inb
  };

  gpiod_line* left_in1  = gpiod_chip_get_line(chip_, left_ina);
  gpiod_line* left_in2  = gpiod_chip_get_line(chip_, left_inb);
  gpiod_line* right_in1 = gpiod_chip_get_line(chip_, right_ina);
  gpiod_line* right_in2 = gpiod_chip_get_line(chip_, right_inb);

  if (!request_output_line(left_in1, "left_ina") ||
      !request_output_line(left_in2, "left_inb") ||
      !request_output_line(right_in1, "right_ina") ||
      !request_output_line(right_in2, "right_inb"))
  {
    RCLCPP_ERROR(
      rclcpp::get_logger("Pca9685SystemHardware"),
      "H-bridge pins not defined");
  }

  motor_in1_.push_back(left_in1);
  motor_in1_.push_back(right_in1);

  motor_in2_.push_back(left_in2);
  motor_in2_.push_back(right_in2);

  wheel_circumference_ = 2.0 * M_PI * wheel_radius_;

  hw_commands_.assign(info_.joints.size(), 0.0);
  hw_position_.assign(info_.joints.size(), 0.0);
  hw_velocity_.assign(info_.joints.size(), 0.0);
  last_encoder_ticks_.assign(info_.joints.size(), 0);
  encoder_ticks_.assign(info_.joints.size(), 0);
  
  RCLCPP_INFO(rclcpp::get_logger("Pca9685SystemHardware"), "Initialized PCA9685 hardware with %zu joints", info_.joints.size());

  /*
  for (const hardware_interface::ComponentInfo & joint : info_.joints)
  {
    // Pca9685System has one command interface on each output
    if (joint.command_interfaces.size() != 1)
    {
      RCLCPP_FATAL(
        rclcpp::get_logger("Pca9685SystemHardware"),
        "Joint '%s' has %zu command interfaces found. 1 expected.", joint.name.c_str(),
        joint.command_interfaces.size());
      return hardware_interface::CallbackReturn::ERROR;
    }

    if (joint.command_interfaces[0].name != hardware_interface::HW_IF_VELOCITY)
    {
      RCLCPP_FATAL(
        rclcpp::get_logger("Pca9685SystemHardware"),
        "Joint '%s' have %s command interfaces found. '%s' expected.", joint.name.c_str(),
        joint.command_interfaces[0].name.c_str(), hardware_interface::HW_IF_VELOCITY);
      return hardware_interface::CallbackReturn::ERROR;
    }
  }
*/

  return hardware_interface::CallbackReturn::SUCCESS;
}


std::vector<hardware_interface::StateInterface> Pca9685SystemHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  
  for (size_t i = 0u; i < info_.joints.size(); i++)
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

std::vector<hardware_interface::CommandInterface> Pca9685SystemHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (size_t i = 0u; i < info_.joints.size(); i++)
  {
    command_interfaces.emplace_back(info_.joints[i].name,
                                    hardware_interface::HW_IF_VELOCITY,
                                    &hw_commands_[i]);
  }

  return command_interfaces;

}

hardware_interface::CallbackReturn Pca9685SystemHardware::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  std::fill(hw_commands_.begin(), hw_commands_.end(), 0.0);
  std::fill(hw_position_.begin(), hw_position_.end(), 0.0);
  std::fill(hw_velocity_.begin(), hw_velocity_.end(), 0.0);

  RCLCPP_INFO(rclcpp::get_logger("Pca9685SystemHardware"), "Successfully activated!");
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn Pca9685SystemHardware::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  for (size_t i = 0; i < hw_commands_.size(); i++)
  {
    pca.set_pwm(i, 0, 0);
  }

  return hardware_interface::CallbackReturn::SUCCESS;

}
//
hardware_interface::return_type Pca9685SystemHardware::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & period)
{
  double dt = period.seconds();
  if (dt <= 0.0)
    return hardware_interface::return_type::OK;

  encoder_ticks_[0] = read_left_encoder();
  encoder_ticks_[1] = read_right_encoder();


  double ticks_per_rev = ticks_per_rev_;
  
  for (size_t i = 0; i < info_.joints.size(); i++)
  {
    int delta_ticks = static_cast<int32_t>(encoder_ticks_[i] - last_encoder_ticks_[i]);

    double delta_revolutions = static_cast<double>(delta_ticks) / ticks_per_rev;

    double delta_radians = delta_revolutions * 2.0 * M_PI;

    hw_position_[i] += delta_radians;

    hw_velocity_[i] = delta_radians / dt;
    //hw_velocity_[i] = 0.8 * hw_velocity_[i] + 0.2 * (delta_pos / dt);

    last_encoder_ticks_[i] = encoder_ticks_[i];
  }

  return hardware_interface::return_type::OK;
}

hardware_interface::return_type Pca9685SystemHardware::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{

for (size_t i = 0u; i < hw_commands_.size(); i++)
{
    double command = hw_commands_[i];

    // Clamp incoming velocity command
    command = std::clamp(command, -max_velocity_, max_velocity_);

    // Normalize to [-1, 1]
    double normalized = command / max_velocity_;

    normalized = std::clamp(normalized, -1.0, 1.0);

    // Small deadband
    if (std::abs(normalized) < 0.05)
    {
      normalized = 0.0;
    }

    int in1 = 0;
    int in2 = 0;

    if (normalized > 0.0)
    {
      in1 = 1;
      in2 = 0;
    }
    else if (normalized < 0.0)
    {
      in1 = 0;
      in2 = 1;
    }
    else
    {
      // Coast
      in1 = 0;
      in2 = 0;
    }

    gpiod_line_set_value(motor_in1_[i], in1);
    gpiod_line_set_value(motor_in2_[i], in2);

    double duty_cycle = std::abs(normalized);

    int pwm_value = static_cast<int>(duty_cycle * 4095.0);

    pwm_value = std::clamp(pwm_value, 0, 4095);

    pca.set_pwm(i, 0, pwm_value);
  }
  return hardware_interface::return_type::OK;
}

bool Pca9685SystemHardware::request_output_line(gpiod_line* line, const std::string& name)
{
  if (!line)
  {
    RCLCPP_ERROR(
      rclcpp::get_logger("Pca9685SystemHardware"),
      "Failed to get GPIO line for %s",
      name.c_str());
    return false;
  }

  int ret = gpiod_line_request_output(
    line,
    "pca9685_ros2_control",
    0);

  if (ret < 0)
  {
    RCLCPP_ERROR(
      rclcpp::get_logger("Pca9685SystemHardware"),
      "Failed to request GPIO output for %s",
      name.c_str());
    return false;
  }

  return true;
}

Pca9685SystemHardware::~Pca9685SystemHardware()
{
  for (auto line : motor_in1_)
  {
    gpiod_line_release(line);
  }

  for (auto line : motor_in2_)
  {
    gpiod_line_release(line);
  }

  if (chip_)
  {
    gpiod_chip_close(chip_);
  }
}

}  // namespace pca9685_hardware_interface

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(
  pca9685_hardware_interface::Pca9685SystemHardware, hardware_interface::SystemInterface)
