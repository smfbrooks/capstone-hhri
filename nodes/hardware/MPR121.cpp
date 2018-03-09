/*	MPR121 Touch Sensor Hardware Node	*/

#include <stdio.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vector>
#include <iostream>

#include "ros/ros.h"
#include "hbs2/i2c_bus.h"

// Perform soft reset and start with MPR121 in "Stop mode" to prevent random reads.
bool write_init(ros::ServiceClient &client, hbs2::i2c_bus &srv) {
    srv.request.request.resize(4);
    srv.request.request = {0x02, 0x5A, 0x80, 0x63};
    srv.request.size = 4;

    if (client.call(srv)) {
		srv.request.request.resize(4);
		srv.request.request = {0x02, 0x5A, 0x5E, 0x00};
		srv.request.size = 4;

		if (client.call(srv)) {
			return true;
		}
    }

    ROS_ERROR("Failed to call i2c_srv");
    return false;
}

// Set up touch and release threshold registers.
bool touch_init(ros::ServiceClient &client, hbs2::i2c_bus &srv) {
	// Touch and release threshold registers 
	uint8_t thres_reg = 0x41;
	uint8_t rel_reg = 0x42;

	// Touch and release thresholds
	uint8_t touch_thres = 12;
	uint8_t rel_thres = 6;

	srv.request.request.resize(4);
	srv.request.size(4);

    // Set up 13 touch channels with touch threshold = 12
	srv.request.request = {0x02, 0x5A, thres_reg, touch_thres};

	for (int i = 0; i < 12; i++) {
		if (client.call(srv)) {
			thres_reg += i*2;
			srv.request.request = {0x02, 0x5A, thres_reg, touch_thres};
		} else { ROS_ERROR("Failed to call i2c_srv for touch"); return false; }
	}

    // Set up 13 touch channels with release threshold = 6
	srv.request.request = {0x02, 0x5A, rel_reg, rel_thres};

	for (int i = 0; i < 12; i++) {
		if (client.call(srv)) {
			rel_reg += i*2;
			srv.request.request = {0x02, 0x5A, rel_reg, rel_thres};
		} else { ROS_ERROR("Failed to call i2c_srv for touch"); return false; }
	}

    return true;
}

// Set up registers (MHD, NHD, NCL, FDL, Debounce touch & release, Baseline tracking)
bool reg_setup(ros::ServiceClient &client, hbs2::i2c_bus &srv) {
    srv.request.request.resize(4);
    srv.request.size = 4;
    srv.request.request = {0x02, 0x5A, 0x00, 0x00};

    // MHD rising reg 0x2B, NHD rising reg 0x2C, NCL rising reg 0x2D, FDL rising reg 0x2E
    srv.request.request.data.at(2) = 0x2B; srv.request.request.data.at(3) = 0x01;
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x2C; srv.request.request.data.at(3) = 0x01; 
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x2D; srv.request.request.data.at(3) = 0x0E; 
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x2E; srv.request.request.data.at(3) = 0x00; 
    if (!client.call(srv)) { return false; }

    // MHD falling reg, 0x2F, NHD falling reg 0x30, NCL falling reg 0x31, FDL falling reg 0x32
    srv.request.request.data.at(2) = 0x2F; srv.request.request.data.at(3) = 0x01; 
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x30; srv.request.request.data.at(3) = 0x05; 
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x31; srv.request.request.data.at(3) = 0x01; 
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x32; srv.request.request.data.at(3) = 0x00; 
    if (!client.call(srv)) { return false; }

    // NHD touched reg 0x33, NCL touched 0x34, FDL touched 0x35
    srv.request.request.data.at(2) = 0x33; srv.request.request.data.at(3) = 0x00; 
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x34; srv.request.request.data.at(3) = 0x00; 
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x35; srv.request.request.data.at(3) = 0x00; 
    if (!client.call(srv)) { return false; }

    // Debounce touch & release reg 0x5B, config 1 reg 0x5c, config 2 ref 0x5D
    srv.request.request.data.at(2) = 0x5B; srv.request.request.data.at(3) = 0x00; 
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x5C; srv.request.request.data.at(3) = 0x10; // 16uA current
    if (!client.call(srv)) { return false; }
    srv.request.request.data.at(2) = 0x5D; srv.request.request.data.at(3) = 0x20; // 0.5 us encoding
    if (!client.call(srv)) { return false; }

    return true;
} 

uint16_t report_touch(ros::ServiceClient &client, hbs2::i2c_bus &srv) {
    uint16_t wasTouched = 0x0000, readTouch[2] = {0x0000}, currentlyTouched = 0;
    srv.request.request.resize(3);
    srv.request.size(3);

    while(1) {
    // Touch status register = 0x00
    srv.request.request = {0x02, 0x5A, 0x00};
    if (client.call(srv)) {
        srv.request.request{0x01, 0x5A, 0x00, 0x00, 0x00, 0x00};
        if (client.call(srv)) {
            readTouch[0] = (uint16_t)(srv.response.data.at(0) << 8);
            readTouch[0] |= (uint16_t)(srv.response.data.at(1));
            readTouch[1] = (uint16_t)(srv.response.data.at(2) << 8);
            readTouch[1] |= (uint16_t)(srv.response.data.at(3));

            currentlyTouched = readTouch[0];
            currentlyTouched |= readTouch[1] << 8;
            currentlyTouched &= 0x0FFF;

            for(int i = 0; i < 12; i++) {
                if ((currentlyTouched & (1 << i)) && !(wasTouched & (1 << i)))
                    printf("Pin %i was touched.\n", i);
                if (!(currentlyTouched & (1 << i)) && (wasTouched & (1 << i)))
                    printf("Pin %i was released. \n", i);
            }
            wasTouched = currentlyTouched;
            usleep(50000);
        }
    }
    }
    return 0;
}

int main(int argc, char **argv) {
    // Initialize touch sensor node
    ros::init(argc, argv, "mpr121");
    ros::ServiceClient client = n.serviceClient<hbs2::i2c_bus>("i2c_bus");
    hbs2::i2c_bus srv;

    // Call functions.

    return 0;
}
