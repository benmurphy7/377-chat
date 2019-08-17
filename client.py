from socket import *
import select
import sys
import os
import time

SERVER_NAME = 'localhost'
SERVER_PORT = 3000

#Store commands from file
commands = []

def connect():
    # Create a socket of Address family AF_INET.
    sock = socket(AF_INET, SOCK_STREAM)
    # Client socket connection to the server
    sock.connect((SERVER_NAME, SERVER_PORT))

    return sock


def send(sock, message):
    sock.send(bytearray(message, 'utf-8'))


def recv(sock):
    return sock.recv(1024).decode('utf-8')


def list(sock):
    send(sock, '-')
    result = recv(sock).rstrip()
    if result == '':
        return []
    else:
        return result.split(',')[0:-1]


def last_list(sock):
    xs = list(sock)
    if len(xs) == 0:
        return ''
    else:
        return xs[-1]


def ask(prompt=':-p'):
    return input(f'({prompt}) ')


def prompt_on_last(sock):
    last = last_list(sock)
    if last == '':
        return ask()
    else:
        return ask(last)

def script(input):
    sockets_list = [sys.stdin, connection]
    read_sockets, write_socket, error_socket = select.select(sockets_list, [], [])

    for socks in read_sockets:
        #Incoming message ready to read
        if socks == connection:
            message = socks.recv(2048).decode('utf-8','ignore')
            #Check connection
            if message == "":
                exit()
                #connection = connect()
            print(">>>%s" % message.strip())

        #Send message
        else:
            message = input
            send(connection, message)
            print(message)
            #print(message)
        time.sleep(1)
def client():
    #Executes scripted commands (if reading from file)
    for command in commands:
        script(command)

    while True:

        #Maintains a list of possible input streams
        sockets_list = [sys.stdin, connection]
        read_sockets, write_socket, error_socket = select.select(sockets_list, [], [])

        for socks in read_sockets:
            #Incoming message ready to read
            if socks == connection:
                message = socks.recv(2048).decode('utf-8','ignore')
                #Check connection
                if message == "":
                    exit()
                    #connection = connect()
                print(">>>%s" % message.strip())

            #Send message
            else:
                message = input()
                send(connection, message)
                #print(message)

def read():
    global commands
    filename = sys.argv[1]
    commands = [line.rstrip('\n') for line in open(filename)]

if len(sys.argv) > 1:
    read()
connection = connect()
client()
