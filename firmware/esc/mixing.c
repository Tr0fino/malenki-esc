#include <stdint.h>
#include <stdlib.h>

#include "mixing.h"
#include "motors.h"
#include "diag.h"
#include "nvconfig.h"
#include "state.h"

mixing_state_t mixing_state;

static int signedclamp(int n, int maxval)
{
    if (n > maxval) return maxval;
    if (n < -maxval) return -maxval;
    return n;
}

static int deadzone(int n, int deadzone)
{
    if ((n < deadzone) && (n > (-deadzone))) {
        return 0;
    }
    return n;
}

// Take a channel in the range -200 .. 200
static void squaring(int *channel, int maxval)
{
    int32_t c32 = *channel;

    // This takes it in the range 200*200 (=40k) 
    c32 *= abs(c32);
    
    c32 /= maxval;
    *channel = (int) c32;
}

// Time to repeat (centiseconds)
#define AUTO_FIRE_INTERVAL 50

static uint16_t axe_process(bool automatic)
{
    uint32_t now = get_tickcount();
    uint32_t interval = now - mixing_state.fire_time;
    if ((interval >= AUTO_FIRE_INTERVAL) || mixing_state.fire_ready) {
        // New shot
        mixing_state.fire_time = now;
        interval = 0;
        mixing_state.fire_ready = false;
    }
    // This is the sequence of events for the axe.
    if (interval < 15) {
        // Maximum forward! Fire!
        return 200;
    }
    if (interval < 20) {
        // Wait a little
        return 0;
    }
    if (interval < 50) {
        // Retract
        return -120;
    }
    if (interval < 100) {
        // Retract more gently
        return -25;
    }
    return 0; // Finished
}


static uint16_t apply_weapon_rules_axe(int16_t throttle, int16_t steering, int16_t weapon)
{
    // weapon is now in range (-200 .. 200)
    // handle retraction normally.
    if (weapon < -20) {
        return signedclamp(weapon,200);
    }
    if (weapon > 50) {
        // Single shot fire
        return axe_process(true);
    } else if (weapon > 150) {
        // Full automatic!
        return axe_process(false);
    } else {
        // Null, reset the state for single shot.
        mixing_state.fire_ready = true;
        return 0;
    }
}

static uint16_t apply_weapon_rules(int16_t throttle, int16_t steering, int16_t weapon)
{
    // Fix up weapon to correct range 
    weapon = (weapon * 20) / 45; // -200 ... 200
    uint8_t mode  = nvconfig_state.weapon_mode;

    if (mode == WEAPON_MODE_FLIPHARD) {
        if (weapon > 70) {
            // threshold to give maximum flip power.
            weapon = 200;
        } else if (weapon < -70 ) {
            // retract with a dead zone, but gradually.
            weapon = signedclamp(weapon, 200);
        } else {
            // dead zone
            weapon = 0;
            // If we are driving, let's auto-retract the weapon.
            if (abs(throttle) > 20) {
                weapon = -40;
            }
        }
    } else if (mode == WEAPON_MODE_AXE1) {
        weapon = apply_weapon_rules_axe(throttle, steering, weapon);
    } else {
        // Default weapon mode
        weapon = deadzone(weapon, 20);
        weapon = signedclamp(weapon, 200);
    }
    return weapon;
}

static uint16_t diag_count=0;

void mixing_drive_motors(int16_t throttle, int16_t steering, int16_t weapon, bool invert)
{
    // inputs are already calibrated to centre on zero.
    // Inputs should be in (approx) microseconds of pulse.
    // Throttle, steering and weapon range is typically +- 450 full range.
    
    // Scale throttle to range +- 100
    throttle = (throttle * 10) / 45;
    // Scale steering to range +- 100
    steering = (steering * 10) / 45;
    // Clamp
    throttle = signedclamp(throttle, 100);  
    steering = signedclamp(steering, 100);  
    
    throttle = deadzone(throttle, 10);    
    steering = deadzone(steering, 10);    
    // Apply "squaring"
    squaring(&throttle,100);
    squaring(&steering,100);
    // Scale steering further, to stop steering too fast.
    steering = steering / 2;
    
    int left = throttle + steering;   
    int right = throttle - steering;   
   
    // left / right should now have range -150 ... 150
    // Clamp them again at 100
    left = signedclamp(left, 100); 
    right = signedclamp(right, 100); 
    // Now scale back to += 200 which is correct for pwm
    left = left *2;
    right = right * 2;
   
    // Apply "squaring"
    // squaring(&left,200);
    // squaring(&right,200);
  
    weapon = apply_weapon_rules(throttle, steering, weapon);
    
    if (invert) {
        int temp;
        // Swap left and right and invert.
        temp = left;
        left = -right;
        right = -temp; 
    }
    // Check nvconfig and invert channels
    if (nvconfig_state.invert_left)
        left = -left;
    if (nvconfig_state.invert_right)
        right = -right;
    if (nvconfig_state.invert_weapon)
        weapon = -weapon;

    set_motor_direction_duty(MOTOR_WEAPON, weapon);
    if (left == 0) {
        enable_motor_brake(MOTOR_LEFT);
    } else {
        set_motor_direction_duty(MOTOR_LEFT, left);
    }
    if (right == 0) {
        enable_motor_brake(MOTOR_RIGHT);
    } else {
        set_motor_direction_duty(MOTOR_RIGHT, right);
    }

    if (diag_count == 0) {
        diag_count = 10;
        diag_println("L: %03d R: %03d W: %03d", left, right, weapon);
    } else {
        --diag_count;
    }
}


void mixing_init()
{
     // Called at startup.
     mixing_reset();
 }
 
void mixing_reset()
{
    mixing_state.fire_ready = true;
    mixing_state.fire_time = 0;
}
