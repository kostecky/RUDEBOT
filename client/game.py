
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
BUTTON_REPEAT_DELAY = 0.001
I_DRIVE_REPEAT_DELAY = 0.001
NAIVE_MOVE_SPEED = '100'
NAIVE_HALF_SPEED = '050'
TICK_SLEEP = 1   # milliseconds
BAIL_TIMEOUT = 2000   # milliseconds - how long to wait for network ACK before assuming connection is dead
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
last_buttons = {}
for k in SIXAXIS_MAP['buttons']:
    last_buttons[k] = 0
last_axes = {}
for k in SIXAXIS_MAP['axes']:
    last_axes[k] = 0

# elements are "name": ((host, port), preshutdown_msg, greeting_msg, keepalive, 'greeting_msg_to_wait_before_sending_our_greeting', 'greeting_to_wait_for_after_sending_our_greeting, cmd_ack, initial_cmd)
# WARNING: If cmd_ack is not None, you MUST also specify initial_cmd!
#socket_defs = {'neck': (NECK_ADDY, None, ' ', ' ', None, None, None, None), 'rover': (ROVER_ADDY, '\\', 'C', '+000\0+000\n', None, 'C', 'C', '+000\0+000\n')}
#socket_defs = {'neck': (NECK_ADDY, None, ' ', ' ', None, None, None, None)}
socket_defs = {'rover': (ROVER_ADDY, '\\', 'C', '+000\0+000\n', None, 'C', 'C', '+000\0+000\n')}
sockets = {}
last_responses = {}
last_keepalive = time.time()

surface = None
# Unused as of yet...
turtle_pos = [400,300]

neck_pos = 90   # For display/info purposes only
neck_buffer = ''

rover_moving = False   # Detect release of controls and trigger a single explicit 'stop' command immediately


for name, defn in socket_defs.items():
    if defn[6] is None and not defn[7] is None:
        raise Exception('You MUST specify an initial_cmd if you specify cmd_ack!')


def zero_pad(msg, size=3):
    msg = str(msg)

    while len(msg) < size:
        msg = '0%s' % msg

    return msg

def bready(name, now):
    if (now - last_buttons[name]) > BUTTON_REPEAT_DELAY and six.get_button(SIXAXIS_MAP['buttons'][name]):
        last_buttons[name] = now

        return True

    return False

def aready(name, now):
    if (now - last_axes[name]) > I_DRIVE_REPEAT_DELAY and 0.1 <= abs(geta(name)):
        last_axes[name] = now

        return True

    return False

def dirready(now):
    if reduce(lambda x,y: x and y, map(lambda x: abs(geta(x)) < 0.1, ('left_y', 'right_y')), True):
        if reduce(lambda x,y: x and y, map(lambda x: not getb(x), ('up', 'down', 'left', 'right')), True):
            return False

    return True

def getb(name):
    return six.get_button(SIXAXIS_MAP['buttons'][name])

def geta(name):
    return six.get_axis(SIXAXIS_MAP['axes'][name])

def stop_i_drive():
    global rover_moving

    if rover_moving:
        rover_moving = False

        rover_cmd('+000\0+000\n')

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
    #pygame.display.init()
    #pygame.display.set_mode((800,600))
    #surface = pygame.display.get_surface()
    #pygame.draw.polygon(surface, pygame.Color('#00FF00FF'), [(400, 300), (500, 300), (450, 213.4)])
    #pygame.draw.polygon(surface, pygame.Color('#FF0000FF'), [(425, 256.7), (475, 256.7), (450, 213.4)])
    #pygame.display.flip()


def socket_kill(name):
    if name in sockets and sockets[name] is not None:
        debug('killing socket %s (fileno: %s)' % (name, sockets[name].fileno()))

        if socket_defs[name][1] is not None:
            try:
                debug('Preshutdown msg on "%s" socket: %s' % (name, socket_defs[name][1]))

                sockets_send(name, msg, rescue=False, reconnect=False)
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

def socket_wait_for(name, msg, buf=''):
    start = time.time()
    while 0 > buf.find(msg):
        #debug('looping in socket_wait_for')
        try:
            buf = '%s%s' % (buf, sockets[name].recv(1))   # Waited messages are sucked down one byte at a time!
        except Exception, e:
            pass

        if ((time.time() - start) * 1000) > BAIL_TIMEOUT:
            raise Exception('Could not connect to server for socket "%s", buf: "%s"' % (name, buf))

        time.sleep(0.001)

    return buf

def socket_reconnect(name):
    socket_kill(name)

    sockets[name] = create_connection(name)

    if socket_defs[name][2] is not None:
        if socket_defs[name][4]is not None:
            try:
                socket_wait_for(name, socket_defs[name][4])
            except Exception, e:
                print('Could not reconnect to %s' % name)
                sys.exit(1)

        socket_send(name, socket_defs[name][2])

        if socket_defs[name][5] is not None:
            try:
                socket_wait_for(name, socket_defs[name][5])
            except Exception, e:
                # If there's no initial command, we can't just assume a clean reconnect, so fail...
                if socket_defs[name][7] is None:
                    print('Could not reconnect to %s' % name)
                    sys.exit(2)

    if socket_defs[name][7] is not None:
        socket_send(name, socket_defs[name][7], rescue=False)

def create_connection(name):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    debug('Connecting to socket %s' % name)
    debug('Connecting to %s:%s' % socket_defs[name][0])

    sock.connect(socket_defs[name][0])

    debug('%s:%s - fileno: %s' % (socket_defs[name][0][0], socket_defs[name][0][1], sock.fileno()))
    debug('setting socket options for %s' % sock.fileno())

    sock.setblocking(0)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    return sock

def socket_send(name, msg, rescue=True, reconnect=True):
    debug('M:%s:%s:%s' % (name, time.time(), msg))

    if len(msg) > 1024:
        raise Exception('Sorry, you tried to send more than 1kb through a socket, this is unsupported (yeah it IS dumb, but the implementation is fast ;).')

    if sockets[name] is None and reconnect:
        socket_reconnect(name)

    try:
        start = time.time()
        sent = 0
        while sent < len(msg):
            #debug('looping in socket_send')
            if 0 < sent:
                time.sleep(0.001)    # 10ms wait for socket overhead if we can't send immediately
                if ((time.time() - start) * 1000) > BAIL_TIMEOUT:
                    raise Exception('Sending timeout!')
            sent += sockets[name].send(msg[sent:])
    except Exception, e:
        if rescue:
            socket_reconnect(name)

            socket_send(name, msg, rescue=False)
        else:
            raise e

def socket_get(name):
    buf = ''

    try:
        buf = sockets[name].recv(1024)
    except Exception, e:
        pass   # exceptions for nonblocking sockets are expected

    debug('socket "%s" received: %s' % (name, buf))

    return buf

def socket_cmd(name, cmd):
    res = socket_get(name)

    if socket_defs[name][6] is not None and 0 > res.find(socket_defs[name][6]):
        # Wait for the last 
        res = socket_wait_for(name, socket_defs[name][6], res)

    socket_send(name, cmd)

    debug('Sent %s to "%s"' % (cmd, name))

    return res
        

def neck_cmd(cmd, timeout=BAIL_TIMEOUT):
    global neck_pos, neck_buffer

    change = NECK_INCREMENT_DEGREES
    if 'a' == cmd:   # left
        change *= -1

    last_msg = socket_cmd('neck', cmd)
    neck_buffer = '%s%s' % (neck_buffer, last_msg)

    if re.search('pos: [0-9]{1,3}\n', neck_buffer):
        last_responses['neck'] = time.time()

        expr1 = re.compile('.*pos: ', flags=(re.DOTALL|re.MULTILINE))
        expr2 = re.compile('\n.*', flags=(re.DOTALL|re.MULTILINE))
        neck_buffer = re.sub(expr2, '', re.sub(expr1, '', neck_buffer))

        last_post = int(neck_buffer) + change
        if last_pos < MAX_NECK_LEFT or last_pos > MAX_NECK_RIGHT:
            last_pos -= change
        neck_pos = last_pos

        debug('final last neck position (current): %s' % neck_pos)

    elif ((time.time() - last_responses['neck']) * 1000) > timeout:
        last_responses['neck'] = time.time()

        socket_reconnect('neck')

        neck_cmd(cmd)


def rover_cmd(cmd):
    socket_cmd('rover', cmd)


for name in socket_defs:
    socket_reconnect(name)
    last_responses[name] = time.time()


init_pygame()

while True:
    # Frame sleep (remove/play with this to speed up the client overall)
    time.sleep(TICK_SLEEP / 1000.0)

    # Send keepalives...
    if (time.time() - last_keepalive) > 1.0:
        for name in sockets:
            if socket_defs[name][3] is not None:
                socket_send(name, socket_defs[name][3])

        last_keepalive = time.time()

    # Process events
    pygame.event.pump()

    # Frame timestamp
    now = time.time()

    # Process input

    ### Control mapping:
    ###
    ### PS button => quit/exit client
    ### d pad => naive drive mode for rover (forward/reverse/turn left/turn right)
    ### bottom triggers => move neck left/right
    ### analog sticks, Y axes => independent drive (run left & right motors in forward/reverse)

    if bready('playstation', now):
        break   # PS button == exit/quit

    # neck
    if bready('l_trigger_bottom', now) or bready('r_trigger_bottom', now):
        if getb('l_trigger_bottom'):
            neck_cmd('a')
        else:
            neck_cmd('d')
            
    # rover - analog sticks (independent drive mode)

    # send explicit stop if sticks are untouched
    if not dirready(now):
        stop_i_drive()
    else:
        rover_moving = True

    # analog sticks take precedence over d-pad...
    if aready('left_y', now) or aready('right_y', now):
        left_y = geta('left_y')
        if abs(left_y) < 0.1:
            left_y = 0

        right_y = geta('right_y')
        if abs(right_y) < 0.1:
            right_y = 0

        cmd_left = '%s\0' % (zero_pad(abs(int(geta('right_y') * 150)) + 50))
        cmd_right = '%s\n' % (zero_pad(abs(int(geta('left_y') * 150)) + 50))

        if right_y < 0:
            cmd_left = '+%s' % cmd_left
        else:
            cmd_left = '-%s' % cmd_left

        if left_y < 0:
            cmd_right = '+%s' % cmd_right
        else:
            cmd_right = '-%s' % cmd_right

        cmd = '%s%s' % (cmd_left, cmd_right)

        rover_cmd(cmd)

    # naive drive mode (using d-pad) - turns take precedence (no 'diagonal' controls right now)

    # turn (or "diagonal" (advancing turn))
    elif bready('left', now) or bready('right', now):
        if getb('left'):
            if getb('up'):
                rover_cmd('+%s\0+%s\n' % (NAIVE_MOVE_SPEED, NAIVE_HALF_SPEED))
            elif getb('down'):
                rover_cmd('-%s\0-%s\n' % (NAIVE_MOVE_SPEED, NAIVE_HALF_SPEED))
            else:
                rover_cmd('+%s\0-%s\n' % (NAIVE_MOVE_SPEED, NAIVE_MOVE_SPEED))
        else:
            if getb('up'):
                rover_cmd('+%s\0+%s\n' % (NAIVE_HALF_SPEED, NAIVE_MOVE_SPEED))
            elif getb('down'):
                rover_cmd('-%s\0-%s\n' % (NAIVE_HALF_SPEED, NAIVE_MOVE_SPEED))
            else:
                rover_cmd('-%s\0+%s\n' % (NAIVE_MOVE_SPEED, NAIVE_MOVE_SPEED))

    # forward/reverse
    elif bready('up', now) or bready('down', now):
        if getb('up'):
            rover_cmd('+%s\0+%s\n' % (NAIVE_MOVE_SPEED, NAIVE_MOVE_SPEED))
        else:
            rover_cmd('-%s\0-%s\n' % (NAIVE_MOVE_SPEED, NAIVE_MOVE_SPEED))
            

for sock in sockets:
    socket_kill(sock)

sys.exit(0)

