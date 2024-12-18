#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    float x;
    float z;
    float theta;
    char status[10];
} RobotData;

int parseRobotData(const char *input, RobotData *data) {
    // Parse the input string and populate the RobotData structure
    int parsed = sscanf(input, "%d,%f,%f,%f,%9s", &data->id, &data->x, &data->z, &data->theta, data->status);

    // Check if all fields were successfully parsed
    if (parsed == 5) {
        return 1; // Success
    } else {
        return 0; // Parsing failed
    }
}

// Example Call
int main() {
    const char *input = "10,30.40,40.30,90.00,inactive";
    RobotData data;

    if (parseRobotData(input, &data)) {
        printf("ID: %d\n", data.id);
        printf("X: %.2f\n", data.x);
        printf("Z: %.2f\n", data.z);
        printf("Theta: %.2f\n", data.theta);
        printf("Status: %s\n", data.status);
    } else {
        printf("Failed to parse robot data.\n");
    }

    return 0;
}