import json
import urllib.parse
import boto3
import os

# Import the native Alu C++ engine compiled via Pybind11
import alu_python

s3 = boto3.client('s3')

def lambda_handler(event, context):
    """
    AWS Lambda handler that triggers on an S3 upload.
    Downloads the image, uses the native C++ Alu engine to process it,
    and uploads the compressed version to a destination bucket.
    """
    # Get the source bucket and key from the S3 event
    bucket = event['Records'][0]['s3']['bucket']['name']
    key = urllib.parse.unquote_plus(event['Records'][0]['s3']['object']['key'], encoding='utf-8')
    
    download_path = f'/tmp/{key}'
    upload_path = f'/tmp/processed-{key}'
    
    try:
        print(f"Downloading {key} from bucket {bucket}")
        s3.download_file(bucket, key, download_path)
        
        print("Processing image natively via Alu Engine...")
        
        # Load, process, and save directly in C++ via Pybind11
        img = alu_python.AluImage(download_path)
        img.grayscale()
        img.save_jpg(upload_path, 80)
        
        dest_bucket = os.environ['DEST_BUCKET']
        dest_key = f'processed/{key}'
        
        print(f"Uploading to {dest_bucket}/{dest_key}")
        s3.upload_file(upload_path, dest_bucket, dest_key)
        
        return {
            'statusCode': 200,
            'body': json.dumps('Image processed successfully via Alu Native Engine!')
        }
        
    except Exception as e:
        print(f"Error processing image {key}: {str(e)}")
        raise e
