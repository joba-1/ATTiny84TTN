#!/usr/bin/env python
 
import json
import requests
import time
import calendar
import sys


# Log to contain the whole JSON objects from TTN
log_file = '/var/log/ttn/thp84.log'


# InfluxDB parameters
server = 'localhost'
port = 8086
database='ttn'
url = 'http://{}:{}/write?db={}&precision=s'.format(server, port, database)


def ttn2influx(ttn_json_string):
    # parse utc timestamp from TTN into time tuple
    data = json.loads(ttn_json_string)
    ts = time.strptime(data["metadata"]["time"].split('.', 1)[0], '%Y-%m-%dT%H:%M:%S')

    # find gateway with best RSSI
    rssi = -999
    for gw in data["metadata"]["gateways"]:
        if gw["rssi"] > rssi:
            rssi = gw["rssi"]
            gtw_id = gw["gtw_id"]

    # build InfluxDB insert string with explicit utc time in seconds resolution
    msg = 'measurements,app_id="{}",dev_id="{}" temp_degC={},vcc_V={},pres_hPa={},humi_Percent={},gtw_id="{}",rssi={} {}'.format(
        data["app_id"], data["dev_id"], data["payload_fields"]["temp_degC"], data["payload_fields"]["vcc_V"],
        data["payload_fields"]["pres_hPa"], data["payload_fields"]["humi_Percent"], gtw_id, rssi, int(calendar.timegm(ts)))

    # send insert request to the database
    result = requests.post(url, data=msg)
    result.raise_for_status()
    return result


# WSGI interface for apache
def application(environ, start_response):
    output = 'OK'
    try:
        if environ["REQUEST_METHOD"] == "POST":

            # get data as received from TTN
            request_body_size = int(environ['CONTENT_LENGTH'])
            request_body = environ['wsgi.input'].read(request_body_size)
            body_string = "".join(chr(b) for b in request_body)

            # write received data to log file
            with open(log_file, 'a') as log:
                log.write(body_string)
                log.write('\n')

            # put selected data into influxdb
            ttn2influx(body_string)

    except Exception as e:
        output = "Exception: " + str(e)

    # send OK to the sender (usually TTN)
    status = '200 OK'
    response_headers = [('Content-type', 'text/plain'),
                        ('Content-Length', str(len(output)))]
    start_response(status, response_headers)

    return [output.encode('Utf-8')]

# CLI interface
if __name__ == "__main__":
    counter = 0
    for line in sys.stdin:
        counter = counter + 1
        try:
            ttn2influx(line)
        except Exception as e:
            print("Line {}: Exception '{}' caused by {}\n".format(counter, str(e), line))
