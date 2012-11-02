
"""Controller code allowing remote, robotic avatar controls via playstation wireless sixaxis controller."""

import operator
import re
import socket
import sys
import time

import pygame


MAX_NECK_LEFT = 30
MAX_NECK_RIGHT = 140
MOVE_COMMAND_DELAY = 0.1
TICK_SLEEP = 10   # milliseconds
BAIL_TIMEOUT = 1000   # milliseconds - how long to wait for network ACK before assuming connection is dead
I_SLEEP_INIT = 10   # milliseconds - initial value for networking loop sleep...
NECK_ADDY = ('192.168.20.127', 7777)
ROVER_ADDY = ('192.168.20.128', 8888)
NECK_READY = 'Good to go'
DEBUG = True
SIXAXIS_MAP = {
  'buttons': {
    'start':            3,
    'select':           0,
    'l_stick':          1,
    'r_stick':          2,
    'up':               4,
    'down':             6,
    'left':             7,
    'right':            5,
    'l_trigger_top':    10,
    'l_trigger_bottom': 8,
    'r_trigger_top':    11,
    'r_trigger_bottom': 9,
    'tri':              12,
    'cir':              13,
    'x':                14,
    'sq':               15,
    'playstation':      16,
  },
  'axes': {
    'left_x':  0,
    'left_y':  1,
    'right_x': 2,
    'right_y': 3,
  }
}

six = None
neck = None
rover = None


def debug(msg):
    if DEBUG:
        print(msg)


def init_pygame():
    global six

    pygame.init()

    if not pygame.joystick.get_init():
        pygame.joystick.init()

    for i in xrange(pygame.joystick.get_count()):
        js = pygame.joystick.Joystick(i)

        if not js.get_init():
            js.init()

        if re.search('playstation.*3', js.get_name(), flags=re.I):
            six = js

            break

    if six is None:
        raise Exception('Could not initialize Sixaxis controller!')

    pygame.display.init()
    pygame.display.set_mode((800,600))


def socket_kill(sock, preshutdown_msg=None):
    if not socket is None:
        if not preshutdown_msg is None:
            try:
                sock.sendall(preshutdown_msg)
            except Exception, e:
                pass

        try:
            sock.shutdown()
        except Exception, e:
            pass
        finally:
            try:
                sock.close()
            except Exception, e:
                pass


def rover_reconnect(hostport=ROVER_ADDY, recon=False):
    global rover

    socket_kill(rover, '\\')

    #rover = socket.socket(socket.AF_INET, socket.SOCK_RAW)
    rover = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    rover.connect(hostport)
    rover.setblocking(0)
    rover.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    buf = ''
    start = time.time()
    i_sleep = I_SLEEP_INIT

    while 0 > buf.find('Hello'):
        try:
            buf = '%s%s' % (buf, rover.recv(1024))
        except Exception, e:
            pass

        if ((time.time() - start) * 1000) > (BAIL_TIMEOUT * 4):   # Rover takes a while to connect (arduino)
            break

        time.sleep(i_sleep / 1000.0)
        i_sleep += I_SLEEP_INIT

    if 0 > buf.find('Hello'):
        if not recon:
            rover_reconnect(hostport, recon=True)
        else:
            sys.exit(2)


def neck_reconnect(hostport=NECK_ADDY, recon=False):
    global neck

    socket_kill(neck)

    neck = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    neck.connect(hostport)
    neck.setblocking(0)

    neck_send('\n', do_reconnect=False)

    if 0 > neck_get().find(NECK_READY):
        if not recon:
            neck_reconnect(hostport, recon=True)
        else:
            sys.exit(1)


def neck_connect():
    neck_send('\n')

    return neck_get()


def neck_cmd(cmd):
    neck_send(cmd)
    neck_get()


def rover_send(msg, do_reconnect=True):
    if rover is None:
        rover_reconnect()

    sent = 0

    try:
        sent = rover.send(msg)

    except Exception, e:
        if do_reconnect:
            rover_reconnect()

            sent = rover.send(msg)
        else:
            raise e

    if sent < len(msg):
        sys.exit(3)


def neck_send(msg, do_reconnect=True):
    if neck is None:
        neck_reconnect()

    try:
        neck.sendall(msg)

    except Exception, e:
        if do_reconnect:
            neck_reconnect()

            neck.sendall(msg)
        else:
            raise e


def neck_get(do_reconnect=True):
    start = time.time()
    i_sleep = I_SLEEP_INIT
    buf = ''

    while 0 > buf.find(NECK_READY):
        try:
            buf = '%s%s' % (buf, neck.recv(1024))
        except Exception, e:
            pass   # Ignore recv fails on non-blocking socket!

        if ((time.time() - start) * 1000) > BAIL_TIMEOUT:
            break

        time.sleep(i_sleep / 1000.0)
        i_sleep += I_SLEEP_INIT

    if 0 > buf.find(NECK_READY) and do_reconnect:
        neck_reconnect()

        return neck_get(do_reconnect=False)

    return buf


neck_connect()
rover_reconnect()

init_pygame()

last = {
  'f': 0,
  'b': 0,
  'l': 0,
  'r': 0,
}

neck_cmd('90')
neck_pos = 90

def move_neck_right(amt=20):
    global neck_pos

    neck_pos += amt

    if neck_pos > MAX_NECK_RIGHT:
        neck_pos = MAX_NECK_RIGHT

    neck_cmd(str(neck_pos))

def move_neck_left(amt=20):
    global neck_pos

    neck_pos -= amt

    if neck_pos < MAX_NECK_LEFT:
        neck_pos = MAX_NECK_LEFT

    neck_cmd(str(neck_pos))
  

last_keepalive = time.time()
last_move_cmd = ''

while True:
    time.sleep(TICK_SLEEP / 1000.0)

    if (time.time() - last_keepalive) > 1.0:
        # Send rover connection keepalive...
        rover_send(' ')
        last_keepalive = time.time()

    pygame.event.pump()

    if 1 == six.get_button(SIXAXIS_MAP['buttons']['playstation']):
        break   # PS button == exit

    now = time.time()

    if six.get_button(SIXAXIS_MAP['buttons']['cir']):
        move_neck_right()
    elif six.get_button(SIXAXIS_MAP['buttons']['sq']):
        move_neck_left()
    else:
        neck = six.get_axis(SIXAXIS_MAP['axes']['right_x'])
        if abs(neck) > 0.1:
            if 0 < neck:
                # turn head right
                move = neck_pos + 10
    
                if neck > 0.75:
                    move += 35
                elif neck > 0.5:
                    move += 20
                elif neck > 0.25:
                    move += 10
    
                if move > MAX_NECK_RIGHT:
                    move = MAX_NECK_RIGHT
    
                neck_cmd(str(move))
                neck_pos = move
            else:
                # turn head left
                move = neck_pos - 10
    
                if neck < -0.75:
                    move -= 35
                elif neck < -0.5:
                    move -= 20
                elif neck < -0.25:
                    move -= 10
    
                if move < MAX_NECK_LEFT:
                    move = MAX_NECK_LEFT
    
                neck_cmd(str(move))
                neck_pos = move

    turn = six.get_axis(SIXAXIS_MAP['axes']['left_x'])
    if six.get_button(SIXAXIS_MAP['buttons']['left']):
        turn = -0.3
    elif six.get_button(SIXAXIS_MAP['buttons']['right']):
        turn = 0.3

    dir = six.get_axis(SIXAXIS_MAP['axes']['left_y']) * -1.0
    if six.get_button(SIXAXIS_MAP['buttons']['up']):
        dir = 0.3
    elif six.get_button(SIXAXIS_MAP['buttons']['down']):
        dir = -0.3

    if abs(turn) > 0.1:
        now = time.time()

        speed = 1

        if abs(turn) > 0.75:
            speed = 4
        elif abs(turn) > 0.5:
            speed = 3
        elif abs(turn) > 0.25:
            speed = 2

        if 0 > turn:
            if (now - last['l']) < MOVE_COMMAND_DELAY:
                continue

            last['l'] = now
            last_keepalive = now
            cmd = 'h'
        else:
            if (now - last['r']) < MOVE_COMMAND_DELAY:
                continue

            last['r'] = now
            last_keepalive = now
            cmd = 'l'

        rover_send(str(speed))
        rover_send(str(cmd))
    # Turns (above) take precedence)
    elif abs(dir) > 0.1:
        now = time.time()

        speed = 1

        if abs(dir) > 0.75:
            speed = 4
        elif abs(dir) > 0.5:
            speed = 3
        elif abs(dir) > 0.25:
            speed = 2

        if 0 > dir:
            if (now - last['b']) < MOVE_COMMAND_DELAY:
                continue

            last['b'] = now
            last_keepalive = now
            cmd = 'j'
        else:
            if (now - last['f']) < MOVE_COMMAND_DELAY:
                continue

            last['f'] = now
            last_keepalive = now
            cmd = 'k'

        rover_send(str(speed))
        rover_send(str(cmd))


for sock in (neck, rover):
    socket_kill(sock)

sys.exit(0)

