#ifndef IMOTOR_H
#define IMOTOR_H

/**
 * @file IMotor.h
 * @brief Abstract base class (interface) for motor control
 * 
 * This interface allows the motion control system to work with different
 * motor types (Stepper, Servo, etc.) through polymorphism.
 */

class IMotor {
public:
    /**
     * @brief Initialize the motor hardware
     * Must be called before any other operations
     */
    virtual void init() = 0;
    
    /**
     * @brief Set the motor speed
     * @param speed Speed in appropriate units (steps/s for steppers, RPM for servos)
     */
    virtual void setSpeed(float speed) = 0;
    
    /**
     * @brief Move to an absolute angle
     * @param angle Target angle in degrees (0-360)
     */
    virtual void moveToAngle(float angle) = 0;
    
    /**
     * @brief Get the current motor angle
     * @return Current angle in degrees
     */
    virtual float getCurrentAngle() = 0;
    
    /**
     * @brief Enable the motor (power on)
     */
    virtual void enable() = 0;
    
    /**
     * @brief Disable the motor (power off)
     */
    virtual void disable() = 0;
    
    /**
     * @brief Check if motor is enabled
     * @return true if enabled, false otherwise
     */
    virtual bool isEnabled() = 0;
    
    /**
     * @brief Check if motor is currently moving
     * @return true if moving, false if stationary
     */
    virtual bool isMoving() = 0;
    
    /**
     * @brief Stop the motor immediately (emergency stop)
     */
    virtual void stop() = 0;
    
    /**
     * @brief Update motor state (call this in the control loop)
     * For steppers: generates steps
     * For servos: updates PWM if needed
     */
    virtual void update() = 0;
    
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~IMotor() = default;
};

#endif // IMOTOR_H
