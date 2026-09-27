"""
@file       BattleBoatsPC.py

GUI for interracting with BattleBoats running on an STM32 Nucleo F411RE uC.

@date       10 May 2025
@version    1.0.1

@todo       Ensure that all daemons properly shut down, i.e. put in a
            "cleanUp()".
"""
try:
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *
    from PyQt5.QtWidgets import *
    from PyQt5.QtWidgets import QWidget # TODO(nubby): Redundant?
    import sys
    import enum
    import functools
    import random

    import serial
    import serial.tools.list_ports
    import threading
    import queue
    import time
except ImportError:
    import sys
    print("This tool requires the following packages, please install them with via your system package manager")
    print("Or (less preferable), the following commands entered into a terminal below")
    print("On Windows: assuming python is on the path (you may need admin)")
    print("python -m pip install PyQt5 pyserial")
    print("On Posix (some issues might occur during QT installation):")
    print("python3 -m pip install PyQt5 pyserial")
    print("OS X suggestion is to use homebrew to do this")
    print("linux suggestion is to follow first steps of guide here")
    print("https://gist.github.com/ujjwal96/1dcd57542bdaf3c9d1b0dd526ccd44ff")
    sys.exit(0)

# package installation
# serial incoming display

fieldCols = 10
fieldRows = 6

def beefHash(inNumber):
    return (inNumber * inNumber) % 0xBEEF


def parity(inNumber):
    return sum([1 for i in range(16) if (inNumber >> i) & 1]) & 1

class messageTypes(enum.Enum):
    CHA = enum.auto()
    ACC = enum.auto()
    REV = enum.auto()
    SHO = enum.auto()
    RES = enum.auto()

    def __str__(self):
        return str(self.name)

class AgentStates(enum.Enum):
    START = enum.auto()
    CHALLENGING = enum.auto()
    ACCEPTING = enum.auto()
    ATTACKING = enum.auto()
    DEFENDING = enum.auto()
    WAITING_TO_SEND = enum.auto()
    END_SCREEN = enum.auto()

class BattleBoatsEvents(enum.Enum):
    NO_EVENT = enum.auto()
    START_BUTTON = enum.auto()
    RESET_BUTTON = enum.auto()
    CHA_RECEIVED = enum.auto()
    ACC_RECEIVED = enum.auto()
    REV_RECEIVED = enum.auto()
    SHO_RECEIVED = enum.auto()
    RES_RECEIVED = enum.auto()
    MESSAGE_SENT = enum.auto()
    ERROR = enum.auto()
    SQUARE_CHOSEN = enum.auto()


class FieldStatus(enum.Enum):
    FIELD_SQUARE_EMPTY = enum.auto()
    FIELD_SQUARE_SMALL_BOAT = enum.auto()
    FIELD_SQUARE_MEDIUM_BOAT = enum.auto()
    FIELD_SQUARE_LARGE_BOAT = enum.auto()
    FIELD_SQUARE_HUGE_BOAT = enum.auto()
    FIELD_SQUARE_UNKNOWN = enum.auto()
    FIELD_SQUARE_HIT = enum.auto()
    FIELD_SQUARE_MISS = enum.auto()
    FIELD_SQUARE_CURSOR = enum.auto()
    FIELD_SQUARE_INVALID = enum.auto()

class shotResults(enum.Enum):
    RESULT_MISS = 0
    RESULT_HIT = 1
    RESULT_SMALL_BOAT_SUNK = 2
    RESULT_MEDIUM_BOAT_SUNK = 3
    RESULT_LARGE_BOAT_SUNK = 4
    RESULT_HUGE_BOAT_SUNK = 5


fieldSymbols = {FieldStatus.FIELD_SQUARE_EMPTY: " ",
                FieldStatus.FIELD_SQUARE_SMALL_BOAT: "3",
                FieldStatus.FIELD_SQUARE_MEDIUM_BOAT: "4",
                FieldStatus.FIELD_SQUARE_LARGE_BOAT: "5",
                FieldStatus.FIELD_SQUARE_HUGE_BOAT: "6",
                FieldStatus.FIELD_SQUARE_UNKNOWN: "?",
                FieldStatus.FIELD_SQUARE_HIT: "X",
                FieldStatus.FIELD_SQUARE_MISS: "M",
                FieldStatus.FIELD_SQUARE_CURSOR: " ",
                FieldStatus.FIELD_SQUARE_INVALID: " "}

""" _check_for_loopback(current_message, previous_message)

Compare a received message with the previously-transmitted message to see if it
is a loopback message.

@param  current_message
@param  previous_message    (str)
@return (bool)  Is it a loopback?
"""
def _check_for_loopback(current_message, previous_message):
    current_message_list = [str(part) for part in current_message]
    try:
        previous_message_list = [
                str(part.strip("$").split("*")[0]
                    ) for part in previous_message.split(",")
                ]
    # If no previous message has beenr recorded, not a loopback.
    except AttributeError:
        return False

    for member in zip(current_message_list, previous_message_list):
        try:
            if member[0] != member[1]:
                return False
        except:
            break
    return True


class Message(object):
    def __init__(self, eventQueue, rawQueue):

        self.outputQueue = queue.Queue()
        self.outputThread = None
        self.receiveThread = None
        self.prevMessage = None
        # TODO(nubby): unused?
        #self.receivedPackets = queue.Queue()
        self.activeConnection = False
        self.serialPort = None
        self.eventQueue = eventQueue
        self.rawQueue = rawQueue

        # Deprecate?
        if len(self.listSerialPorts()) > 0:
            self.Port = self.listSerialPorts()[0]
        else:
            self.Port = None

        # Connect to the player's device and start the IO daemons.
        #self.Connect()

    """_startIODaemons()
    
    Spool up daemons for processing input and output messages over UART.
    """
    def _startIODaemons(self):
        # Start the buffered output thread.
        self.outputThread = threading.Thread(target=self.handleOutGoing, name='outgoingHandler')
        self.outputThread.daemon = True
        self.outputThread.start()

        # Start the buffered input thread.
        self.receiveThread = threading.Thread(
            target=self.handleIncoming, name='incomingHandler'
        )
        self.receiveThread.daemon = True
        self.receiveThread.start()

    def handleOutGoing(self):
        while True:
            if self.activeConnection:
                try:
                    newByte = self.outputQueue.get()
                    self.serialPort.write(newByte.to_bytes(1, byteorder='big'))
                    # Flush input buffer to account for loopback.
                    # NOTE: This currently discards returned data as well, so 
                    #       we opt for a more "brute-force" check.
                    #self.serialPort.reset_input_buffer()
                    time.sleep(0.03)
                except serial.serialutil.SerialException as e:
                    pass
            else:
                time.sleep(0.01)
        return

    def handleIncoming(self):
        """This is the threaded instance that actually handles the protocol"""
        rawQueue = list()

        while True:
            if not self.activeConnection:
                time.sleep(0.1)
                continue
            try:
                datum = self.serialPort.read(1).decode('ascii')
                rawQueue.append(datum)
                #print(str(datum))
                self.rawQueue.put(datum)
            except serial.serialutil.SerialException as e:
                continue
            if rawQueue[0] != '$':  # we pop items until the first one is a $ sign
                val = rawQueue.pop(0)
            if ('\n' in rawQueue) and (len(rawQueue) > 0): # we assume with the \n we have a valid message
                rawQueue.pop(0) # remove the $
                try:
                    rawPayload = rawQueue[0:rawQueue.index("*")]
                except ValueError as e:
                    print(e)
                    continue
                stringPayload = "".join(rawPayload)
                valueList = stringPayload.split(",")
                try:
                    for i in range(1, len(valueList)):
                        valueList[i] = int(valueList[i])
                    valueList[0] = messageTypes[valueList[0]]
                except (ValueError, KeyError):
                    continue
                if not _check_for_loopback(valueList, self.prevMessage):
                    self.eventQueue.put(valueList, block=True)
                rawQueue.clear()
                while not self.rawQueue.empty():
                    self.rawQueue.get()
                # we are going to ignore checksums for now
            time.sleep(0.01)


    def Connect(self):
        # we will handle the no port case here
        if self.Port is None:
            for potPort in serial.tools.list_ports.comports():
                if potPort.device == 'COM1':
                    continue
                self.Port = potPort.device
        if self.activeConnection:
            return False
        if self.Port is None:
            self.activeConnection = False
            return False
        else:
            try:
                # open the serial port
                self.serialPort = serial.Serial()
                self.serialPort.baudrate = '115200'
                self.serialPort.port = self.Port
                self.serialPort.dtr = None
                self.serialPort.open()
                self.activeConnection = True
                self._startIODaemons()
                return True
            except serial.serialutil.SerialException:
                print('Invalid Port')
                pass
        return False

    def Disconnect(self):
        if self.activeConnection:
            self.activeConnection = False
            self.serialPort.close()
            return True
        else:
            return False


    @staticmethod
    def listSerialPorts():
        return [potPort.device for potPort in serial.tools.list_ports.comports()]

    def checksum(self, string):
        checksum = 0
        for char in string:
            checksum = checksum ^ ord(char)
        return checksum

    def encode_message(self, Fields):
        payload = ",".join([str(x) for x in Fields])
        checksum = self.checksum(payload)
        #message = "${}*{:02X}\r\n".format(payload, checksum)
        message = "${}*{:02X}\r\n".format(payload, checksum)
        return message
        # checksum = 0
        # for letter in message:
        #   checksum = checksum ^ ord(letter)
        # return "$%s*%02X" % (message, checksum)

    def sendMessage(self, messageToSend):
        encodedMessage = self.encode_message(messageToSend)
        self.prevMessage = encodedMessage
        for char in encodedMessage:
            self.outputQueue.put(ord(char))
        return encodedMessage



class Field(QWidget):
    def __init__(self, fieldFill, callBackonPush, parent=None):
        super().__init__(parent)
        buttonGrid = QGridLayout()


        self.defaultFill = fieldFill

        self.setLayout(buttonGrid)

        self.boatLives = [3, 4, 5, 6]

        self.fieldArray = list()
        for row in range(fieldRows):
            self.fieldArray.append(list())
            for col in range(fieldCols):
                newButton = QPushButton(fieldSymbols[self.defaultFill])
                newButton.fieldStatus = self.defaultFill
                newButton.clicked.connect(functools.partial(callBackonPush, row, col))
                self.fieldArray[-1].append(newButton)
                buttonGrid.addWidget(newButton, row, col)


    def setFieldStatus(self, row, col, newStatus):
        oldStatus = self.fieldArray[row][col]
        self.fieldArray[row][col].fieldStatus = newStatus
        self.fieldArray[row][col].setText(fieldSymbols[newStatus])
        return oldStatus

    def getFieldStatus(self, row, col):
        return self.fieldArray[row][col].fieldStatus

    def resetField(self):
        self.boatLives= [3, 4, 5, 6]
        for row in range(fieldRows):
            for col in range(fieldCols):
                self.setFieldStatus(row, col, self.defaultFill)


    def placeBoats(self):

        for size, type in zip([6, 5, 4, 3],[FieldStatus.FIELD_SQUARE_HUGE_BOAT, FieldStatus.FIELD_SQUARE_LARGE_BOAT, FieldStatus.FIELD_SQUARE_MEDIUM_BOAT, FieldStatus.FIELD_SQUARE_SMALL_BOAT]):
            while True:
                direction = random.randrange(2)
                # direction = 0
                if direction:
                    startXPoint = random.randrange(0, fieldCols)
                    if size != 6:
                        startYPoint = random.randrange(0, fieldRows - (size))
                    else:
                        startYPoint = 0

                    validPosition = True
                    for pointToTest in range(size):
                        if self.getFieldStatus(pointToTest+startYPoint, startXPoint) != FieldStatus.FIELD_SQUARE_EMPTY:
                            validPosition = False
                    if validPosition:
                        for i in range(size):
                            self.setFieldStatus(startYPoint+i, startXPoint, type)
                        break

                else:
                    startXPoint = random.randrange(0, fieldCols - (size))
                    startYPoint = random.randrange(0, fieldRows)

                    validPosition = True
                    for pointToTest in range(size):
                        if self.getFieldStatus(startYPoint, pointToTest+startXPoint) != FieldStatus.FIELD_SQUARE_EMPTY:
                            validPosition = False
                    if validPosition:
                        for i in range(size):
                            self.setFieldStatus(startYPoint, startXPoint+i, type)
                        break
        return

    def setFieldButtonsDisabled(self, state):
        for row in self.fieldArray:
            for col in row:
                col.setDisabled(state)


"""mainInterface(QMainWindow)

Class defining core GUI functionality for playing BattleBoats.

@param  QMainWindow - 
"""
class mainInterface(QMainWindow):
    tempStatusSignal = pyqtSignal(str)
    def __init__(
            self,
            parent=None,
            title="ECE013 BattleBoats Interface"
        ):
        super(mainInterface, self).__init__(parent)

        # TODO(nubby): Deprecate?
        # self.setMinimumHeight(720)
        # self.setMinimumWidth(1280)
        # self.resize(1280, 720)

        self.agentState = AgentStates.START
        self.eventQueue = queue.Queue()
        self.rawQueue = queue.Queue()

        self.guessData = [0, 0]
        self.turnCount = 0
        self.turnOrder = False

        self.challengeData = {"A": 0, "#a": 0, "B": 0}

        self._setup_sm_timeout()

        self.messageObject = Message(self.eventQueue, self.rawQueue)

        self.setWindowTitle(title)
        # self.mainWindow = mainWidget(None)
        # self.statusBar().showMessage("Active Connection: {}".format("hi"), 1000)

        self.mainLayout = QVBoxLayout()
        self.mainWidget = QWidget()
        self.mainWidget.setLayout(self.mainLayout)
        self.setCentralWidget(self.mainWidget)

        self.topControlsBox = QHBoxLayout()
        self.mainLayout.addLayout(self.topControlsBox)

        self.openSerialButton = QPushButton("Open Serial")
        self.openSerialButton.clicked.connect(self.openPort)
        self.topControlsBox.addWidget(self.openSerialButton)

        self.closeSerialButton = QPushButton("Close Serial")
        self.closeSerialButton.clicked.connect(self.closePort)
        self.topControlsBox.addWidget(self.closeSerialButton)

        self.serialPortLine = QComboBox()
        self.findSerialPorts()
        self.topControlsBox.addWidget(self.serialPortLine)

        self.findSerialPortButton = QPushButton("Find Serial Port")
        self.findSerialPortButton.clicked.connect(self.findSerialPorts)
        self.topControlsBox.addWidget(self.findSerialPortButton)

        self.serialStatus = QLabel("")
        self.topControlsBox.addWidget(self.serialStatus)
        self.updateSerialStatus()

        self.resetButton = QPushButton("Reset")
        self.topControlsBox.addWidget(self.resetButton)
        self.resetButton.clicked.connect(self.resetGame)
        # self.resetButton.clicked.connect(self.resetFields)

        self.sendChallengeButton = QPushButton("Send Challenge")
        self.topControlsBox.addWidget(self.sendChallengeButton)
        self.sendChallengeButton.clicked.connect(self.sendChallenge)
        self.topControlsBox.addStretch()


        self.gameStatusBox = QHBoxLayout()
        self.mainLayout.addLayout(self.gameStatusBox)

        self.gameStatusBox.addWidget(QLabel("Turn Count: "))
        self.turnCountLabel = QLabel("0")
        self.gameStatusBox.addWidget(self.turnCountLabel)
        self.gameStatusBox.addSpacing(1)

        self.gameStatusBox.addWidget(QLabel("Turn: "))
        self.turnOrderLabel = QLabel("N/A")
        self.gameStatusBox.addWidget(self.turnOrderLabel)
        self.gameStatusBox.addStretch()


        self.fieldsBox = QHBoxLayout()
        self.mainLayout.addLayout(self.fieldsBox)

        self.myField = Field(FieldStatus.FIELD_SQUARE_EMPTY, self.testCallBack)
        # self.myField.placeBoats()
        self.theirField = Field(FieldStatus.FIELD_SQUARE_UNKNOWN, self.squareEvent)

        self.fieldsBox.addWidget(self.myField)
        self.fieldsBox.addWidget(self.theirField)
        self.fieldsBox.addStretch()

        self.messageBox = QHBoxLayout()

        self.mainLayout.addLayout(self.messageBox)

        outStackBox = QVBoxLayout()
        self.messageBox.addLayout(outStackBox)

        outStackBox.addWidget(QLabel("Outgoing Messages"))
        self.outgoingMessages = QPlainTextEdit()
        self.outgoingMessages.setReadOnly(True)
        outStackBox.addWidget(self.outgoingMessages)

        inStackBox = QVBoxLayout()
        self.messageBox.addLayout(inStackBox)

        inStackBox.addWidget(QLabel("Incoming Messages"))
        self.messageBox.addLayout(inStackBox)
        self.incomingMessages = QPlainTextEdit()
        self.incomingMessages.setReadOnly(True)
        inStackBox.addWidget(self.incomingMessages)

        # self.myField.setFielButtonsDisabled(True)
        self.mainLayout.addStretch()

    """_setup_sm_timeout()

    Configures a Qt timeout for the central BattleBoats state machine.

    @param  timeout (int)   Timeout in ms within which the main state machine
                            must complete its loop.
    """
    def _setup_sm_timeout(self, timeout=100):
        self.SMTimer = QTimer()
        self.SMTimer.timeout.connect(self.runStateMachine)
        self.SMTimer.start(timeout)

    def testCallBack(self, row, col):
        return

    def findSerialPorts(self):
        self.serialPortLine.clear()
        for port in self.messageObject.listSerialPorts():
            self.serialPortLine.addItem(port)
        self.serialPortLine.setCurrentIndex(0)
        return

    def openPort(self):
        self.messageObject.Port = self.serialPortLine.currentText()
        self.messageObject.Connect()
        self.updateSerialStatus()
        return

    def closePort(self):
        self.messageObject.Disconnect()
        self.updateSerialStatus()

    def updateSerialStatus(self):
        if self.messageObject.activeConnection == True:
            self.serialStatus.setText("Serial {} Open".format(self.messageObject.serialPort.port))
        else:
            self.serialStatus.setText("Serial Closed")

    def sendChallenge(self):
        self.eventQueue.put([BattleBoatsEvents.START_BUTTON])

    def resetFields(self):
        self.myField.resetField()
        # self.myField.placeBoats()
        self.theirField.resetField()
        self.theirField.setFieldButtonsDisabled(False)
        return

    def updateOutgoing(self, inString):
        self.outgoingMessages.appendPlainText(inString)
        return

    def updateIncoming(self, inString):
        self.incomingMessages.appendPlainText(inString)
        return

    def resetGame(self):
        self.agentState = AgentStates.START
        # self.myField.resetField()
        # self.theirField.resetField()
        self.resetFields()
        self.turnCount = 0
        self.updateGameData()
        self.turnOrderLabel.setText("")
        self.outgoingMessages.clear()
        self.incomingMessages.clear()

    def runStateMachine(self):
        if not self.rawQueue.empty():
            raw = self.rawQueue.get()
            #print(raw)
        while not self.eventQueue.empty():
            curEvent = self.eventQueue.get()
            if isinstance(curEvent[0], messageTypes):
                self.updateIncoming(self.messageObject.encode_message(curEvent))
            if self.agentState == AgentStates.START:
                if curEvent[0] == BattleBoatsEvents.START_BUTTON:
                    self.challengeData["A"] = random.randint(0, 0xFFFF)
                    self.challengeData["#a"] = beefHash(self.challengeData["A"])
                    messageSent = self.messageObject.sendMessage([messageTypes.CHA, self.challengeData["#a"]])
                    self.updateOutgoing(messageSent)
                    self.resetFields()
                    self.myField.placeBoats()
                    self.agentState = AgentStates.CHALLENGING
                elif curEvent[0] == messageTypes.CHA:
                    self.agentState = AgentStates.ACCEPTING
                    self.challengeData["B"] = random.randint(0, 0xFFF)
                    self.challengeData["#a"] = curEvent[1]
                    messageSent = self.messageObject.sendMessage([messageTypes.ACC, self.challengeData["B"]])
                    self.updateOutgoing(messageSent)
                    self.resetFields()
                    self.myField.placeBoats()
            elif self.agentState == AgentStates.CHALLENGING:
                if curEvent[0] == messageTypes.ACC:
                    messageSent = self.messageObject.sendMessage(
                        [messageTypes.REV, self.challengeData["A"]]
                    )
                    self.updateOutgoing(messageSent)
                    # we now need to determine who goes first and set the state appropriately
                    self.challengeData["B"] = curEvent[1]
                    order = parity(self.challengeData["A"] ^ self.challengeData["B"])
                    if not order:
                        self.agentState = AgentStates.DEFENDING
                        # print('defending')
                    else:
                        self.agentState = AgentStates.WAITING_TO_SEND
                        self.turnOrder = True
                        self.updateGameData()
                        # print('attacking')
                        # self.eventQueue.put([BattleBoatsEvents.NO_EVENT])

                # print('Implement me Please', self.agentState)
            elif self.agentState == AgentStates.ACCEPTING:
                if curEvent[0] == messageTypes.REV:
                    self.challengeData["A"] = curEvent[1]
                    order = parity(self.challengeData["A"] ^ self.challengeData["B"])
                    if order:
                        self.agentState = AgentStates.DEFENDING
                    else:
                        self.agentState = AgentStates.WAITING_TO_SEND
                        self.turnOrder = True
                        self.updateGameData()
                # print('Implement me Please', self.agentState)
            elif self.agentState == AgentStates.ATTACKING:
                if curEvent[0] == messageTypes.RES:
                    row = curEvent[1]
                    col = curEvent[2]
                    result = curEvent[3]
                    if shotResults(result) == shotResults.RESULT_MISS:
                        self.theirField.setFieldStatus(row, col, FieldStatus.FIELD_SQUARE_MISS)
                    else:
                        self.theirField.setFieldStatus(row, col, FieldStatus.FIELD_SQUARE_HIT)

                    if shotResults(result) == shotResults.RESULT_HUGE_BOAT_SUNK:
                        self.theirField.boatLives[3] = 0
                    if shotResults(result) == shotResults.RESULT_LARGE_BOAT_SUNK:
                        self.theirField.boatLives[2] = 0
                    if shotResults(result) == shotResults.RESULT_MEDIUM_BOAT_SUNK:
                        self.theirField.boatLives[1] = 0
                    if shotResults(result) == shotResults.RESULT_SMALL_BOAT_SUNK:
                        self.theirField.boatLives[0] = 0

                    # print('Implement me Please', self.agentState)
                    self.agentState = AgentStates.DEFENDING
                    self.turnOrder = False
                    self.updateGameData()
                    if all([x == 0 for x in self.theirField.boatLives]):
                        self.agentState = AgentStates.END_SCREEN
                        self.eventQueue.put([BattleBoatsEvents.NO_EVENT])


            elif self.agentState == AgentStates.DEFENDING:
                if curEvent[0] == messageTypes.SHO:
                    curSquareStatus = self.myField.getFieldStatus(curEvent[1], curEvent[2])
                    print(curSquareStatus)
                    hitResult = shotResults.RESULT_MISS
                    if curSquareStatus == FieldStatus.FIELD_SQUARE_SMALL_BOAT:
                        self.myField.boatLives[0] -= 1
                        hitResult = shotResults.RESULT_HIT
                        if self.myField.boatLives[0] == 0:
                            hitResult = shotResults.RESULT_SMALL_BOAT_SUNK
                    if curSquareStatus == FieldStatus.FIELD_SQUARE_MEDIUM_BOAT:
                        self.myField.boatLives[1] -= 1
                        hitResult = shotResults.RESULT_HIT
                        if self.myField.boatLives[1] == 0:
                            hitResult = shotResults.RESULT_MEDIUM_BOAT_SUNK
                    if curSquareStatus == FieldStatus.FIELD_SQUARE_LARGE_BOAT:
                        self.myField.boatLives[2] -= 1
                        hitResult = shotResults.RESULT_HIT
                        if self.myField.boatLives[2] == 0:
                            hitResult = shotResults.RESULT_LARGE_BOAT_SUNK
                    if curSquareStatus == FieldStatus.FIELD_SQUARE_HUGE_BOAT:
                        self.myField.boatLives[3] -= 1
                        hitResult = shotResults.RESULT_HIT
                        if self.myField.boatLives[3] == 0:
                            hitResult = shotResults.RESULT_HUGE_BOAT_SUNK
                    # print("Hi", hitResult)
                    messageSent = self.messageObject.sendMessage([messageTypes.RES, curEvent[1], curEvent[2], hitResult.value])
                    if hitResult != shotResults.RESULT_MISS:
                        self.myField.setFieldStatus(curEvent[1], curEvent[2], FieldStatus.FIELD_SQUARE_HIT)
                    else:
                        self.myField.setFieldStatus(curEvent[1], curEvent[2], FieldStatus.FIELD_SQUARE_MISS)
                    self.updateOutgoing(messageSent)
                    self.agentState = AgentStates.WAITING_TO_SEND
                    self.turnOrder = True
                    self.updateGameData()
                    # self.eventQueue.put([BattleBoatsEvents.NO_EVENT])
                    # print('Implement me Please', self.agentState)
            elif self.agentState == AgentStates.WAITING_TO_SEND:
                if curEvent[0] == BattleBoatsEvents.SQUARE_CHOSEN:
                    self.guessData[0] = curEvent[1]
                    self.guessData[1] = curEvent[2]
                    messageSent = self.messageObject.sendMessage([messageTypes.SHO, self.guessData[0], self.guessData[1]])
                    self.updateOutgoing(messageSent)
                    self.turnCount += 1
                    self.updateGameData()
                    self.agentState = AgentStates.ATTACKING
                    # print('Implement me Please', self.agentState)
            elif self.agentState == AgentStates.END_SCREEN:
                if self.turnOrder:
                    self.turnOrderLabel.setText(("You Lose"))
                else:
                    self.turnOrderLabel.setText(("You Win"))

                # print('Implement me Please', self.agentState)
            return

    def squareEvent(self, row, col):
        self.theirField.fieldArray[row][col].setDisabled(True)
        self.eventQueue.put([BattleBoatsEvents.SQUARE_CHOSEN, row, col])
        return

    def updateGameData(self):
        self.turnCountLabel.setText(str(self.turnCount))
        if self.turnOrder:
            self.turnOrderLabel.setText("Yours")
        else:
            self.turnOrderLabel.setText("Theirs")

sys._excepthook = sys.excepthook

def my_exception_hook(exctype, value, tracevalue):
    # Print the error and traceback
    import traceback
    with open("LastCrash.txt", 'w') as f:
        # f.write(repr(exctype))
        # f.write('\n')
        # f.write(repr(value))
        # f.write('\n')
        traceback.print_exception(exctype, value, tracevalue, file=f)
        # traceback.print_tb(tracevalue, file=f)
    print(exctype, value, tracevalue)
    # Call the normal Exception hook after
    sys._excepthook(exctype, value, tracevalue)
    sys.exit(0)

# Set the exception hook to our wrapping function
sys.excepthook = my_exception_hook


# testingMessage = Message()
# print(testingMessage.encode_message([messageTypes.RES, 4, 7, 1]))

app = QApplication(sys.argv)
gui = mainInterface()
gui.show()
app.exec()
