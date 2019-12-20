# PLOTS

This describes some bits and pieces to create plots with Grafana dashboards.

## TTN HTTP Integration

The setup is very simple: go to your TTN application (console, Applications, thp84 in my case). 
Select Integrations, add Integration, HTTP Integration.
Use a name meaningful to you as process id (I used thp84-http) and enter the URL you configured your webserver with, e.g. 

    http://your.web.server/ttn/thp84

Select Add integration and you are done.

## Apache configuration

I wont go through apache webserver setup. That is described elsewhere so many times. Just the addons required:

    WSGIScriptAlias /ttn/thp84      /srv/www/htdocs/ttn/thp84.py
    <Directory /srv/www/htdocs/ttn>
      Require all granted
    </Directory>

This configuration file apache2-ttn.conf in /etc/apache2/conf.d/ allows the TTN HTTP integration to post new sensordata to your webserver.
The URL to use is https://your.web.server/ttn/thp84.
But ist should be straightforward to adapt this to your needs now.
You need to install the apache wsgi module for python 3 and enable it.
Obviously you also need Python 3.

    zypper install python3 apache2-mod_wsgi-python3

## Script Installation

Just copy the thp84.py python script to the directory you have configured above (/srv/www/htdocs/ttn).
It will call the InfluxDB url to insert a new measurement via the WSGI interface.
If you use another logfile, InfluxDB server, port or database name, just edit them at the top of the script.

## Manually Insert Measurements

If you want to import the JSON data from your logfile to the InfluxDB database (e.g. to recreate it from scratch)
You can use the same thp84.py script for that:

    cat /var/log/ttn/thp84.log | python3 thp84.py

## InfluxDB and Grafana

Finally you need an InfluxDB database and Grafana.
Again, there are good tutorials out there, I wont repeat. But a general overview I published here:
https://banzhaf.chickenkiller.com/mediawiki/index.php5/Project_TTN_Daten_in_InfluxDB_f%C3%BCr_Grafana

Once you have InfluxDB running, you have to create the database to store the measurements in. Default in the thp84.py script is "ttn".
Create it with this simple command:

    influx -execute 'create database ttn'

As a starting point for Grafana, you can use the JSON file Grafana-dashboard-TTN-thp84-Device.json, which defines a basic dashboard with the collected data

Have fun...
Joachim

