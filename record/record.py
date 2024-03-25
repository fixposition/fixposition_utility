#!/usr/bin/env python3
########################################################################################################################

import argparse
import inspect
import json
import logging
import re
import signal
import sys
import textwrap
import time
import urllib.parse
import urllib.request

LOG = logging.getLogger(__name__)

def main():
    """Main program"""

    # Command line arguments
    parser = argparse.ArgumentParser(
        description = 'Record log from Vision-RTK 2 sensor over ethernet',
        epilog      = """

        Examples:

            Record medium log:

                ./record.py 10.0.2.1 medium

            Record medium log for approx. 20 seconds:

                timeout -s INT 20 ./record.py 172.22.1.44 medium

        _""", formatter_class=SaneArgparseFormatter)
    parser.add_argument('sensor',
                        help     = 'Sensor hostname or IP address',
                        metavar  = '<sensor>')
    parser.add_argument('profile',
                        help     = 'Recording profile (minimal, medium, maximal, ...)',
                        metavar  = '<profile>')
    parser.add_argument('-v', '--verbose',
                        help     = 'Increase verbosity',
                        default  = 0,
                        action   = 'count')
    args = parser.parse_args()

    # Logging
    log_level = logging.INFO
    if args.verbose > 0:
        log_level = logging.DEBUG
    logging.basicConfig(level=log_level, format=None)
    try:
        import coloredlogs
        coloredlogs.install(logger=LOG, level=log_level, fmt='%(message)s')
    except Exception as ex:
        LOG.debug(f'No colours! {ex}')
    LOG.debug(args)

    # Run app
    try:
        if not FpRecord(args.sensor, args.profile).Run():
            raise Exception('Logging failed')
        LOG.info('Done')
    except Exception as e:
        LOG.error(f'Ouch: {e}')
        sys.exit(1)
    sys.exit(0)

# ----------------------------------------------------------------------------------------------------------------------

class FpRecord:
    """
    Recor a log from a Vision-RTK 2 via network
    """
    def __init__(self, sensor, profile):
        self.sensor = sensor
        self.profile = profile
        self.sensor_api = f'http://{self.sensor}/api/v2'
        self.sensor_dl = f'http://{self.sensor}:21100'
        LOG.debug(f'{self}: {self.sensor_api} {self.sensor_dl}')

    # ------------------------------------------------------------------------------------------------------------------

    def __str__(self):
        return f'{__class__.__name__}({self.sensor}, {self.profile})'

    def _debug(self, s):
        LOG.debug(f'{self}::{inspect.stack()[1][3]}() {s}')

    def _warning(self, s):
        LOG.warning(f'{self}::{inspect.stack()[1][3]}() {s}')

    # ------------------------------------------------------------------------------------------------------------------

    def Run(self):
        # Detect sensor
        sensor_info = self.DetectSensor()
        if sensor_info is None:
            raise Exception(f'Failed detecting sensor!')
        LOG.info(f"Detected {sensor_info['uid']} ({sensor_info['sw_ver']}, {sensor_info['hardware']} {sensor_info['hw_ver']})")

        # Check we can get logging status. If not, the sensor probably isn't compatible
        log_status = self.GetStatus()
        if log_status is None:
            raise Exception(f'This sensor does not appear to support logging over network')

        # Build request to start log download
        req_data = json.dumps(dict(target='download', profile=self.profile)).encode('utf-8')
        req = urllib.request.Request(f'{self.sensor_dl}/start')
        req.add_header('Content-Type', 'application/json')

        success = True
        try:
            # Send request
            res = urllib.request.urlopen(req, req_data)

            # Check response
            content_type = res.headers.get_content_type()
            content_disposition = res.headers.get_content_disposition()
            self._debug(f'content-type: {content_type}, content_disposition: {content_disposition}')
            # - JSON means it didn't like the request
            if content_type == 'application/json':
                res_data = res.read().decode('utf-8')
                self._warning(f'Sensor said: {res_data}')
                return False
            # - Otherwise it should be the log data
            elif (content_type != 'application/octet-stream') or (content_disposition is None) or (content_disposition != 'attachment'):
                self._warning(f'Unexpected content type {content_type}')
                return False

            # Get log filename (from Content-disposition header)
            self.filename = re.findall('filename="(.+)"', str(res.headers))[0]
            LOG.info(f'Downloading {self.filename}, press CTRL-c to stop logging')
            logfile = open(self.filename, 'wb')

            # Install signal handler to stop logging on C-c
            self.abort = False
            signal.signal(signal.SIGINT, self._StopLogging)

            # Download
            chunk_size = 1024 * 1024
            expected_rate = dict(minimal=0.4, medium=0.8, maximal=4.2, calib=14.0)
            if self.profile in expected_rate:
                chunk_size = int(expected_rate[self.profile] * 1024 * 1024 * 2) # ~ 2s of data
            total_size = 0
            t0 = time.time()
            last_chunk = t0
            while not self.abort:
                chunk = res.read(chunk_size)
                now = time.time()

                # Done (or aborted)
                if not chunk:
                    break

                # Write
                logfile.write(chunk)

                # Update download status
                size = len(chunk)
                total_size = total_size + size
                rate = size / (now - last_chunk)
                last_chunk = now
                duration = int(now - t0 + 0.5)
                dur_min = int(duration / 60)
                dur_sec = duration - (dur_min * 60)
                dl_str = f'{self.filename}: duration {dur_min}:{dur_sec:02}, size {total_size/1024/1024:.1f} MiB, rate {rate/1024/1024:.1f} MiB/s'

                # Update logging status
                log_status = self.GetStatus()
                if log_status is not None:
                    state = log_status['state']
                    queue_skip = log_status['queue_skip']
                    log_errors = log_status['log_errors']
                    LOG.info(f'{state} {dl_str} (skip {queue_skip}, errors {log_errors})')
                else:
                    LOG.info(f'??? {dl_str}')

            logfile.close()

        except Exception as ex:
            self._warning(str(ex))
            success = False

        signal.signal(signal.SIGINT, signal.SIG_DFL)

        return success

    # ------------------------------------------------------------------------------------------------------------------

    def _StopLogging(self, signum, frame):
        LOG.info('***** Stop logging. Please wait... *****')
        try:
            with urllib.request.urlopen(f'{self.sensor_api}/record/stop', data=bytes()) as res:
                data = json.loads(res.read())
                self._debug(f'{data}')
                if ('_ok' in data) and data['_ok']:
                    return data
        except Exception as ex:
            self._warning(str(ex))
            self.abort = True

    # ------------------------------------------------------------------------------------------------------------------

    def DetectSensor(self):
        """
        Check that sensor is alive. Returns dict with sensor info or None
        """
        try:
            with urllib.request.urlopen(f'{self.sensor_api}/sys/info') as res:
                data = json.loads(res.read())
                self._debug(f'{data}')
                if ('sw_ver' not in data) and ('release_tag' in data):
                    data['sw_ver'] = data['release_tag'] # < 2.90.0 / 2.85.3
                if ('_ok' in data) and data['_ok'] and ('hardware' in data) and ('hw_ver' in data) and ('sw_ver' in data) and ('uid' in data):
                    return data
        except Exception as ex:
            self._warning(str(ex))
        return None

    # ------------------------------------------------------------------------------------------------------------------

    def GetStatus(self):
        try:
            with urllib.request.urlopen(f'{self.sensor_api}/record/status') as res:
                data = json.loads(res.read())
                self._debug(f'{data}')
                if ('_ok' in data) and data['_ok']:
                    return data
        except Exception as ex:
            self._warning(str(ex))
        return None

# ----------------------------------------------------------------------------------------------------------------------

class SaneArgparseFormatter(argparse.HelpFormatter):
    """Sane argparse formatter that formats description and epilogs in a more usable way than the default s#!7 formatter"""
    # Stolen from https://stackoverflow.com/questions/3853722/python-argparse-how-to-insert-newline-in-the-help-text
    def _fill_text(self, text, width, indent):
        return "\n".join([textwrap.fill(line, width) for line in textwrap.indent(textwrap.dedent(text), indent).splitlines()])

# ----------------------------------------------------------------------------------------------------------------------
# do it..

if __name__ == "__main__":
    main()

########################################################################################################################
