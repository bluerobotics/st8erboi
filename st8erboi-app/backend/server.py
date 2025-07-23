# backend/server.py
from flask import Flask, jsonify
from flask_cors import CORS

app = Flask(__name__)
CORS(app) # Enable CORS for all routes

@app.route('/api/data')
def get_data():
    return jsonify({'message': 'Hello from the Python backend!'})

if __name__ == '__main__':
    app.run(port=5000)