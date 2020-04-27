import subprocess
import os

def handler(query_payload: dict, ctx) -> dict:
  print("Hello Moto")
  cmdline = ["./exec"]
  print("Run CMD: ", cmdline)
  subprocess.check_call(cmdline, shell=False, stderr=subprocess.STDOUT, env={'AWS_PROFILE':'bbdev','AWS_EC2_METADATA_DISABLED':'true'})
  with open('/tmp/parquet-arrow-example.parquet', 'rb') as f:
    first_four = f.read(4)
    print(first_four)