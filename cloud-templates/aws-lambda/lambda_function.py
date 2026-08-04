import os
import json
import base64
import uuid
import alu_python

def lambda_handler(event, context):
    try:
        # Check if body exists and is base64 encoded
        if 'body' not in event:
            return {
                'statusCode': 400,
                'body': json.dumps({'error': 'No image payload provided'})
            }
            
        body = event['body']
        if event.get('isBase64Encoded', False):
            image_data = base64.b64decode(body)
        else:
            image_data = body.encode('utf-8')
            
        temp_in = f"/tmp/{uuid.uuid4().hex}.jpg"
        temp_out = f"/tmp/{uuid.uuid4().hex}_out.jpg"
        
        with open(temp_in, 'wb') as f:
            f.write(image_data)
            
        # Process via native Alu C++ Engine
        alu_img = alu_python.AluImage(temp_in)
        alu_img.grayscale()
        
        success = alu_img.save_jpg(temp_out, 90)
        
        if not success:
            return {
                'statusCode': 500,
                'body': json.dumps({'error': 'Native engine failed to encode image'})
            }
            
        with open(temp_out, 'rb') as f:
            out_data = f.read()
            
        return {
            'statusCode': 200,
            'headers': {
                'Content-Type': 'image/jpeg'
            },
            'body': base64.b64encode(out_data).decode('utf-8'),
            'isBase64Encoded': True
        }
        
    except Exception as e:
        return {
            'statusCode': 500,
            'body': json.dumps({'error': str(e)})
        }
    finally:
        if os.path.exists(temp_in):
            os.remove(temp_in)
        if os.path.exists(temp_out):
            os.remove(temp_out)
