import os
import uuid
from flask import Flask, request, send_file, jsonify
import alu_python

app = Flask(__name__)

@app.route("/process", methods=["POST"])
def process_image():
    if "image" not in request.files:
        return jsonify({"error": "No image file provided"}), 400
    
    file = request.files["image"]
    temp_in = f"/tmp/{uuid.uuid4().hex}.jpg"
    temp_out = f"/tmp/{uuid.uuid4().hex}_out.jpg"
    
    file.save(temp_in)
    
    try:
        # Initialize native Alu C++ Engine via Pybind11
        alu_img = alu_python.AluImage(temp_in)
        
        # Apply mathematically verified processing
        alu_img.grayscale()
        
        # Save output via native engine
        success = alu_img.save_jpg(temp_out, 90)
        
        if not success:
            return jsonify({"error": "Native engine failed to encode image"}), 500
            
        return send_file(temp_out, mimetype="image/jpeg")
        
    except Exception as e:
        return jsonify({"error": str(e)}), 500
    finally:
        # Cleanup temp files
        if os.path.exists(temp_in):
            os.remove(temp_in)
        if os.path.exists(temp_out):
            os.remove(temp_out)

if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0", port=int(os.environ.get("PORT", 8080)))
