
"""Client code allowing remote, robotic avatar controls via playstation wireless sixaxis controller."""

import operator
import re
import socket
import sys
import time

import pygame


DEBUG = True
MAX_NECK_LEFT = 30
MAX_NECK_RIGHT = 140
NECK_INCREMENT_DEGREES = 5
MOVE_COMMAND_DELAY = 0.1
BUTTON_REPEAT_DELAY = 0.1
FLIP_MODE_REPEAT_DELAY = 1
I_DRIVE_REPEAT_DELAY = 0.02
TICK_SLEEP = 10   # milliseconds
BAIL_TIMEOUT = 500   # milliseconds - how long to wait for network ACK before assuming connection is dead
I_SLEEP_INIT = 10   # milliseconds - initial value for networking loop sleep...
NECK_ADDY = ('192.168.20.126', 7777)
ROVER_ADDY = ('192.168.20.128', 8888)
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
# elements are "name": ((host, port), preshutdown_msg, greeting_msg, ready_marker, keepalive)
socket_defs = {'neck': (NECK_ADDY, None, ' ', 'pos:', ' '), 'rover': (ROVER_ADDY, '\\', 'C', 'C', None))
sockets = {}
surface = None
# Unused as of yet...
turtle_pos = [400,300]

last = {
  'f': 0,
  'b': 0,
  'l': 0,
  'r': 0,
  'select': 0,
  'sq': 0,
  'cir': 0,
  'i_left': 0,   # independent_drive left axis
  'i_right': 0,   # independent_drive right axis
}

neck_pos = 90


def zero_pad(msg, size=3):
    while len(msg) < size:
        msg = '0%s' % msg

    return msg

def debug(msg):
    if DEBUG:
        #sys.stdout.write(msg)
        print('%s:%s' % (time.time(), msg))


def init_pygame():
    global six, surface

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

    ### Client graphics
    pygame.display.init()
    pygame.display.set_mode((800,600))
    surface = pygame.display.get_surface()
    pygame.draw.polygon(surface, pygame.Color('#00FF00FF'), [(400, 300), (500, 300), (450, 213.4)])
    pygame.draw.polygon(surface, pygame.Color('#FF0000FF'), [(425, 256.7), (475, 256.7), (450, 213.4)])
    pygame.display.flip()


def socket_kill(name):
    if name in sockets and sockets[name] is not None:
        debug('killing socket %s (fileno: %s)' % (name, sockets[name].fileno()))

        if socket_defs[name][1] is not None:
            try:
                debug('Preshutdown msg on "%s" socket: %s' % (name, socket_defs[name][1]))

                sockets[name].sendall(preshutdown_msg)
            except Exception, e:
                pass

        try:
            debug('Sending socket "%s" for shutdown' % name)

            sockets[name].shutdown()
        except Exception, e:
            pass
        finally:
            try:
                debug('Closing socket "%s"' % name)

                sockets[name].close()
            except Exception, e:
                pass

def socket_reconnect(name):
    socket_kill(name)

    sockets[name] = create_connection(name)

    if socket_defs[name][2] is not None:
        socket_send(name, socket_defs[name][2])

def create_connection(name):
    socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    debug('Connecting to socket %s' % name)
    debug('Connecting to %s:%s' % socket_defs[name][0])

    socket.connect(socket_defs[name][0])

    debug('%s:%s - fileno: %s' % (socket_defs[name][0][0], socket_defs[name][0][1], socket.fileno()))
    debug('setting socket options for %s' % socket.fileno())

    socket.setblocking(0)
    socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    return socket

def socket_send(name, msg, rescue=True):
    debug('M:%s:%s:%s' % (name, time.time(), msg))

    if sockets[name] is None:
        socket_reconnect(name)

    try:
        sockets[name].sendall(msg)
    except Exception e:
        if rescue:
            socket_reconnect(name)

            socket_send(name, msg, rescue=False)
        else:
            raise e

def socket_get(name, expect=None):
    start = time.time()
    i_sleep = I_SLEEP_INIT
    buf = ''

    while 0 > buf.find(expect):
        try:
            res = sockets[name].recv(1024)
            # Slurp whatever's waiting in the socket...
            while len(res) == 1024:
                buf = '%s%s' % (buf, res)
                res = sockets[name].recv(1024)

        except Exception, e:
            pass   # Ignore recv fails on non-blocking socket!

        if ((time.time() - start) * 1000) > BAIL_TIMEOUT:
            break

        time.sleep(i_sleep / 1000.0)

        debug('"get" slept for %s milliseconds total (socket: %s)' % (i_sleep, name))

        i_sleep += I_SLEEP_INIT

    debug('socket "%s" received: %s' % (name, buf))

    return buf

def socket_cmd(name, cmd, recon=True):
    res = socket_get(name, socket_defs[name][3])

    if 0 > res.find(socket_defs[name][3]):
        if recon:
            socket_reconnect(name)

            return socket_cmd(name, cmd, recon=False)
        else:
            raise Exception('Could not retrieve buffer containing "ready" flag for socket "%s"' % name)
    else:
        socket_send(cmd)

    return res
        

def neck_cmd(cmd):
    global neck_pos

    change = NECK_INCREMENT_DEGREES
    if 'a' == cmd:   # left
        change *= -1

    last_pos = socket_cmd('neck', cmd)

    expr1 = re.compile('.*pos: ', flags=(re.DOTALL|re.MULTILINE))
    expr2 = re.compile('\n.*', flags=(re.DOTALL|re.MULTILINE))
    pos = re.sub(expr2, '', re.sub(expr1, '', last_pos))

    last_pos += change
    if last_pos < MAX_NECK_LEFT or last_pos > MAX_NECK_RIGHT:
        last_pos -= change
    neck_pos = last_pos

    debug('final last neck position (current): %s' % neck_pos)


def rover_cmd(cmd):
    socket_cmd('rover', cmd)


# Compound/secondary commands/etc
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
  

for def in socket_defs:
    socket_reconnect(def)
    
init_pygame()


# HERE


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


    if six.get_button(SIXAXIS_MAP['buttons']['select']) and (now - last['select']) > FLIP_MODE_REPEAT_DELAY:
        last['select'] = now

        flip_mode()

    if six.get_button(SIXAXIS_MAP['buttons']['cir']) and (now - last['cir']) > BUTTON_REPEAT_DELAY:
        last['cir'] = now

        move_neck_right()

    elif six.get_button(SIXAXIS_MAP['buttons']['sq']) and (now - last['sq']) > BUTTON_REPEAT_DELAY:
        last['sq'] = now

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

    if mode == 'independent_drive':
        left = six.get_axis(SIXAXIS_MAP['axes']['left_y']) * -1.0
        right = six.get_axis(SIXAXIS_MAP['axes']['right_y']) * -1.0

        # X and Y (left and right) are swapped motor-wise right now, ie: the first/left parameter in the command controls the right motor
        # As such, ''cmd_str_left'' below is the first half of the command string, and controls the right motor
        cmd_str_left = '+000\0'
        cmd_str_right = '+000\n'
        queued = False

        if abs(left) > 0.1 and (now - last['i_left']) > I_DRIVE_REPEAT_DELAY:
            last['i_left'] = now
            queued = True

            cmd_str_right = '%s\n' % zero_pad(str(int(abs(left) * 200)))

            if 0 > left:
                cmd_str_right = '-%s' % cmd_str_right
            else:
                cmd_str_right = '+%s' % cmd_str_right

        if abs(right) > 0.1 and (now - last['i_right']) > I_DRIVE_REPEAT_DELAY:
            last['i_right'] = now
            queued = True

            cmd_str_left = '%s\n' % zero_pad(str(int(abs(right) * 200)))

            if 0 > right:
                cmd_str_left = '-%s' % cmd_str_left
            else:
                cmd_str_left = '+%s' % cmd_str_left

        if queued:
            cmd_str = '%s%s' % (cmd_str_left, cmd_str_right)

            #print(cmd_str)   # Debugging
            rover_send(cmd_str)
    else:
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


for sock in sockets:
    socket_kill(sock)

sys.exit(0)

