import streamlit as st
import numpy as np
import matplotlib.pyplot as plt
import json
import pandas as pd
import os

# Function to load and parse JSON data
def load_car_data(json_file_path):
    with open(json_file_path, 'r') as file:
        data = [json.loads(line) for line in file if "robot_id" in line]  # Filter lines containing "robot_id"

    parsed_data = [
        {
            "time": pd.to_datetime(entry.get("timestamp")),
            "robot_id": entry.get("robot_id"),
            "x_coord": entry.get("x_coord"),
            "y_coord": entry.get("y_coord"),
            "theta": entry.get("theta"),
        }
        for entry in data
        if "timestamp" in entry and "robot_id" in entry  # Ensure valid entries
    ]

    return pd.DataFrame(parsed_data)

# File path to the car data JSON
file_path = "data/car_data.json"

# Define waypoints for the course
waypoints = [
    {"id": 1, "x": -725, "y": -455},  # Bottom left
    {"id": 2, "x": 745, "y": -467},  # Bottom right
    {"id": 3, "x": -728, "y": 730},  # Top left
    {"id": 4, "x": 765, "y": 748},  # Top right
]

# Convert waypoints into x and y lists
course_x = [wp["x"] for wp in waypoints]
course_y = [wp["y"] for wp in waypoints]

# Close the loop for the course
course_x.append(course_x[0])
course_y.append(course_y[0])

# Title of the application
st.title("Real-Time Car Position and Path Tracking")

try:
    # Load and process the data
    if os.path.exists(file_path):
        car_data = load_car_data(file_path)

        if car_data.empty:
            st.write("No data available for visualization.")
        else:
            # Convert timestamps to naive datetime objects (remove timezone awareness)
            car_data["time"] = car_data["time"].dt.tz_localize(None)

            # Sidebar for display mode selection
            display_mode = st.sidebar.selectbox("Display Mode", ["Real-Time Tracking", "Historical Paths"])

            if display_mode == "Real-Time Tracking":
                # Filter for the latest time
                latest_time = car_data["time"].max()
                filtered_data = car_data[car_data["time"] == latest_time]
            else:
                # Historical paths: Slider to select time range
                st.sidebar.subheader("Historical Time Range")
                time_options = car_data["time"].sort_values().unique()
                start_time, end_time = st.sidebar.select_slider(
                    "Select Time Range",
                    options=time_options,
                    value=(time_options[0], time_options[-1])
                )

                # Filter data within the selected time range
                filtered_data = car_data[(car_data["time"] >= start_time) & (car_data["time"] <= end_time)]

            # Group data by robot ID for path plotting
            grouped_data = filtered_data.groupby("robot_id")

            # Plot the course and the cars' positions
            fig, ax = plt.subplots()
            ax.set_title("Cars' Position and Path on the Course")
            ax.set_xlabel("X Coordinate")
            ax.set_ylabel("Y Coordinate")
            ax.grid()

            # Plot the course with a thicker, solid line
            ax.plot(course_x, course_y, linestyle="-", linewidth=3, color="blue", label="Course")

            # Add waypoints as markers
            for wp in waypoints:
                ax.scatter(wp["x"], wp["y"], color="green", marker="o", zorder=5)

            # Plot paths and current positions for each robot
            for robot_id, robot_data in grouped_data:
                ax.plot(robot_data["x_coord"], robot_data["y_coord"], linestyle="--", label=f"Path Robot {robot_id}")
                current_robot_data = robot_data[robot_data["time"] == robot_data["time"].max()]
                for _, row in current_robot_data.iterrows():
                    ax.scatter(row["x_coord"], row["y_coord"], label=f"Robot {robot_id} (current)", zorder=5)

            # Remove duplicate labels from the legend
            handles, labels = ax.get_legend_handles_labels()
            unique_labels = list(dict(zip(labels, handles)).items())
            ax.legend([item[1] for item in unique_labels], [item[0] for item in unique_labels])

            # Display the plot in Streamlit
            st.pyplot(fig)

            # Show the cars' current positions
            st.subheader("Current Car Positions")
            latest_data = filtered_data[filtered_data["time"] == filtered_data["time"].max()]
            st.write(latest_data[["robot_id", "x_coord", "y_coord", "theta"]])
    else:
        st.error(f"JSON file not found at {file_path}")
except Exception as e:
    st.error(f"Error loading or visualizing data: {e}")
