#I dont think we need this file, I think we just need the node js file

from flask import Flask, request

app = Flask(__name__)

@app.route('/data', methods=['POST'])
def receive_data():
    sensor_value = request.form.get('sensor_value')
    print(f"Received sensor value: {sensor_value}")
    return "Data received", 200

if __name__ == '__main__':
    app.run(host='128.197.55.106', port=5000)
