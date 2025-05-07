import paho.mqtt.client as mqtt
import json
import joblib
import numpy as np

# ----- Configuration -----

# Local MQTT Broker (where sensor data is published)
LOCAL_MQTT_BROKER = "192.168.229.242"  # Change if your local broker is different
LOCAL_MQTT_PORT = 1883
LOCAL_MQTT_TOPIC = "sensor/data"  # Topic used by your sensor publisher

# ThingsBoard MQTT Broker configuration
THINGSBOARD_BROKER = "mqtt.thingsboard.cloud"
THINGSBOARD_PORT = 1883
THINGSBOARD_TOPIC = "v1/devices/me/telemetry"
THINGSBOARD_USERNAME = "QDqngSO0b9LoyZ7O9Fia"  # Your ThingsBoard device token

# Path to the pre-trained Random Forest model (update as needed)
RF_MODEL_PATH = "rf_model.pkl"

# ----- Load the Pre-trained Model -----
try:
    rf_model = joblib.load(RF_MODEL_PATH)
    print("Random Forest model loaded successfully.")
except Exception as e:
    print("Error loading RF model:", e)
    raise

# ----- Define AQI Bucket Assignment Function -----
def assign_aqi_bucket(aqi_value):
    if aqi_value <= 50:
        return 'Good'
    elif aqi_value <= 100:
        return 'Moderate'
    elif aqi_value <= 150:
        return 'Unhealthy for Sensitive Groups'
    elif aqi_value <= 200:
        return 'Unhealthy'
    elif aqi_value <= 300:
        return 'Very Unhealthy'
    else:
        return 'Hazardous'

# ----- Set Up ThingsBoard MQTT Client -----
thingsboard_client = mqtt.Client(client_id="ThingsBoardClient")
thingsboard_client.username_pw_set(THINGSBOARD_USERNAME)
thingsboard_client.connect(THINGSBOARD_BROKER, THINGSBOARD_PORT, keepalive=60)
thingsboard_client.loop_start()

# ----- Local MQTT Client Callback -----
def on_local_message(client, userdata, msg):
    try:
        # Decode the received CSV payload
        payload_str = msg.payload.decode("utf-8")
        print("Received sensor payload:", payload_str)
        
        fields = payload_str.split(',')
        if len(fields) != 7:
            print("Invalid sensor payload format. Expected 7 comma-separated values.")
            return
        
        # Parse sensor readings and convert types
        try:
            temperature = float(fields[0])
            humidity    = float(fields[1])
            mq02        = float(fields[2])
            air_quality = float(fields[3])  # "mp" field mapped to Air Quality
            pm1_0       = float(fields[4])
            pm2_5       = float(fields[5])
            pm10        = float(fields[6])
        except Exception as conv_err:
            print("Error converting sensor values:", conv_err)
            return
        
        # Prepare telemetry data
        telemetry_data = {
            "Temperature (C)": temperature,
            "Humidity (%)": humidity,
            "MQ2": mq02,
            "Air Quality": air_quality,
            "PM1.0": pm1_0,
            "PM2.5": pm2_5,
            "PM10": pm10
        }
        print("Parsed sensor data:", telemetry_data)
        
        # Prepare input for the model
        # Expected features: ['PM2.5', 'PM10', 'NO2', 'NOx', 'NH3', 'CO', 'SO2', 'O3', 'Benzene', 'Toluene', 'Xylene']
        model_input = np.array([[pm2_5, pm10, 0, 0, 0, 0, 0, 0, 0, 0, 0]])
        
        # Predict AQI using the pre-trained model
        predicted_aqi = rf_model.predict(model_input)[0]
        predicted_aqi = float(predicted_aqi)  # Ensure it is a float
        
        # Assign AQI bucket based on predicted value
        aqi_bucket = assign_aqi_bucket(predicted_aqi)
        
        # Add predictions to the telemetry data
        telemetry_data["Predicted AQI"] = predicted_aqi
        telemetry_data["AQI Category"] = aqi_bucket
        
        print("Final Telemetry JSON:", telemetry_data)
        
        # Convert telemetry data to JSON string
        telemetry_json = json.dumps(telemetry_data)
        
        # Publish telemetry data to ThingsBoard
        result = thingsboard_client.publish(THINGSBOARD_TOPIC, telemetry_json, qos=1)
        
        # Debug publish status
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print("Telemetry data published to ThingsBoard successfully.")
        else:
            print("Failed to publish telemetry data to ThingsBoard. Error code:", result.rc)
        
    except Exception as e:
        print("Error processing sensor payload:", e)

# ----- Set Up Local MQTT Client (Sensor Subscriber) -----
local_client = mqtt.Client(client_id="LocalSensorClient")
local_client.on_message = on_local_message

try:
    local_client.connect(LOCAL_MQTT_BROKER, LOCAL_MQTT_PORT, keepalive=60)
except Exception as e:
    print("Error connecting to local MQTT broker:", e)
    raise

local_client.subscribe(LOCAL_MQTT_TOPIC, qos=1)

# Start the local client's network loop (this call is blocking)
print("Starting MQTT loop. Waiting for sensor data...")
local_client.loop_forever()
