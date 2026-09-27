# -*- coding: utf-8 -*-
"""
Created on Wed Aug 22 12:43:24 2018

@author: AutonomousSystemsLab
"""

help_text = """
WELCOME TO AGENT.PY!

Author Max Lichtenstein,  created Aug 2018

This is a tool to assist in grading for UCSC ECE13's BattleBoats lab.  It 
provides wrappers to allow humans to efficiently play as BattleBoats agents
for the purposes of testing and grading student projects.

>>> If you want to use the serial functionality of this tool, you will probably
>>> need to install the python package 'pySerial'

Following is a brief description of the elements of agent.py's UI:

SERIAL CONNECTION PANEL: You can connect via serial to an stm32 agent.  In
theory, the correct port will already be detected, but in practice that's rarely
the case.  Use the text box to manually enter in the port of your Nucleo (you
can use CoolTerm or something to scan for the correct port, it's usually
something like "/dev/tty.usbmodem1234").  

    You may also find it useful to modify the "default_serial" parameter in the
    code.

    Once the correct port is entered, you can press "open serial" button to
    connect!

SERIAL COMMUNICATIONS PANEL: The "incoming" and "outgoing" fields are pretty
self-explanitory, they maintain a record of the serial i/o with the Nucleo. Note
that the outgoing serial is throttled to aid visibility.

    The "prepare outgoing message" fields below can be used to send custom
    messages.  Write a payload and wrap it in "$...*XX", or write a full
    message.

MESSAGE ANALYSIS PANEL: Sometimes, it is helpful to analyze a message in a way
that is challenging for a human.  For example, it is hard to know if a secret
and an A token agree during the negotiation phase, or what the parity of a given
A would be.  This window shows that information, and it can be used to "cheat"
during the negotiation process to go first or second as you please.  This is
helpful for testing all 4 entrances to the agent turn cycle.

NEGOTIATION TOOLS PANEL: These windows keep a record of the negotiation data,
and can also be used to do calculations with negotiation data.  As the game goes
on, the fields fill up with hash, secret (A), and B as the agents reveal these
things.  

    The "Calculate Hash" button will take whatever value is in the "secret"
    field and fill the "hash" field with its 0xbeef hash. The "Calculate Parity"
    button will calculate the parity of whatever is in the Secret and B fields.

    These fields are used when the python agent sends negotiation messages (see
    below)

PLAY PANEL: Negotiation settings (FAIR/FIRST_MOVE/SECOND_MOVE):  When generating
hashes/secrets/Bs for negotiation, the python agent can attempt to cheat as
either accepting or challenging agent.  This is helpful for testing all 4
entrances to the agent turn cycle!  Select a first-move or second-move option to
attempt to go first or second.  Note that this isn't guarenteed to work 100% of
the time, depending on the value of the secret.  But it works for most secrets.

    The send buttons: These are probably where you will spend most of your time!

        "Send CHA" sends a challenge message.  It uses the value in the "hash"
        field.  If no value is present, it uses the secret to generate a hash.
        If no secret is present, it generates one.

        "Send ACC" sends an accept message.  It uses the value in the B field.
        If no value is present, it generates one.

        "Send REV" sends a reveal message.  It uses the value in the secret
        field.  If no value is present, it generates one.

        "Send SHO" randomly picks a square and sends a shot message at that
        square.

        "Send RES" sends a result for the last square.  It picks randomly
        between hits and misses.


Have fun battling!
"""



try:
    import tkinter as tk
    import serial
    import serial.tools.list_ports
    #import time
    import random
    import sys
    #from difflib import SequenceMatcher
    #import traceback
    import re
except ImportError:
    import sys
    print(
            "This tool requires the following packages, please install them with " +
            "the commands entered into a terminal below"
            )
    print("On Windows: assuming python is on the path (you may need admin)")
    print("python -m pip install pyserial")
    print("On Posix ")
    print("python3 -m pip install pyserial")
    print("OS X suggestion is to use homebrew to do this")
    # print(On)
    sys.exit(0)


#config:
refresh_period = 100 #ms
readout_width= 30


testing = False
student_release_mode = False


#serial config:
# CHANGE THE BELOW TO MATCH YOUR SYSTEM:
default_port = "/dev/cu.usbmodem103" #right now we only support 1
baud= 115200
timeout = 0.25


#%% First, the basic hash functions:

def beefHash(A):
    return (A*A) % 0xBEEF

def parity(A):
    return sum([ 1 for i in range(16) if (A>>i) & 1]) & 1

def encode_message(message):
    checksum = 0;
    for letter in message:
        checksum = checksum ^ ord(letter)
    return "$%s*%02X" % (message, checksum)

def crack_hash(hash_a):
    assert(type(hash_a) == int)
    valid_sources = [A for A in range(0xFFFF) if beefHash(A) == hash_a]
    return valid_sources

if testing:
    pass
#	print("Parity of 0b0001101 = ",parity(0b001101))	
#	print("Parity of 0b1001101 = ",parity(0b1001101))
#	
#	print("Beefhash of 3 (9) = ", beefHash(3))
#	print("Beefhash of 221 (48841) = ", beefHash(221))
#	print("Beefhash of 0xDE (405) = ", beefHash(0xDE))
#	print("Beefhash of 0xBEEF (405) = ", beefHash(0xBEEF))
#	
#	print("Beefhash of 12345 = ", beefHash(12345))
#	
#	print("Crackhash of 9 = ", crack_hash(9))


#%%

#%% 

class PyAgent:
    def __init__(self, master):
        self.master = master
        self.master.title("CSE013 PyAgent BattleBoats testing tool")


        serial_status = tk.Frame(master,padx=10,pady=10)
        serial_status.grid(row=0, column=0)
        misc = tk.Frame(master, padx=10, pady=10)
        misc.grid(row=0, column=1)
        io_viewer = tk.Frame(master,padx=10,pady=10)			
        io_viewer.grid(row=1, column=0)
        m_prep = tk.Frame(master,padx=10,pady=10)		
        m_prep.grid(row=2, column=0)	

        tk.Button(
                misc, text="RESET ALL FIELDS", command = self.reset
                ).grid(row=0,column=0)	
        tk.Button(
                misc, text="HELP", command = self.show_help
                ).grid(row=0,column=1)

        analysis_panel = tk.Frame(master,padx=10,pady=10)
        analysis_panel.grid(row=1, column=1)
        autoplay_panel = tk.Frame(master,padx=10,pady=10)
        if not student_release_mode:
            autoplay_panel.grid(row=2, column=1)

        #SERIAL STATUS CONTROLS:
        self.serial_status_label = tk.Label(serial_status, text="SERIAL CLOSED")
        self.serial_status_label.grid(row=0, column=0)		
        self.ser_open_button = tk.Button(
                serial_status, text="Open Serial", command=self.openSer
                )
        self.ser_open_button.grid(row=0, column=1)		
        self.ser_close_button = tk.Button(
                serial_status, text="Close Serial", command=self.closeSer
                )
        self.ser_close_button.grid(row=0, column=2)		
        self.ser_port_box = tk.Entry(serial_status)
        self.ser_port_box.insert(0, default_port)
        self.ser_port_box.grid(row=0, column=3)


        #INPUT AND OUTPUT VIEWERS:		
        tk.Label(io_viewer, text="OUTGOING").grid(row=0, column=0)
        self.raw_serial_out = tk.Text(io_viewer, width=30)
        self.raw_serial_out.grid(row=1,column=0)
        self.raw_out_scroller = tk.Scrollbar(io_viewer, 
                                             command=self.raw_serial_out.yview)
        self.raw_out_scroller.grid(row=1,column=1,  sticky='nsew')

        tk.Label(io_viewer, text="INCOMING").grid(row=0, column=2)
        self.raw_serial_in = tk.Text(io_viewer, width=readout_width)
        self.raw_serial_in.grid(row=1,column=2)
        self.raw_in_scroller = tk.Scrollbar(io_viewer, 
                                            command=self.raw_serial_in.yview)
        self.raw_in_scroller.grid(row=1,column=3, sticky='nsew')

        #MESSAGE PREP PANEL:
        self.payload_to_send = tk.StringVar()
        self.raw_message_to_send = tk.StringVar()

        tk.Label(
                m_prep, text="PREPARE OUTGOING MESSAGE:"
                ).grid(row=0, columnspan=2)
        tk.Label(m_prep, text="FULL MESSAGE:").grid(row=1, column=0)
        tk.Entry(
                m_prep, width=30,textvariable=self.raw_message_to_send
                ).grid(row=1, column=1)
        self.send_button = tk.Button(
                m_prep, text="SEND MESSAGE", command = self.sendRaw
                )
        self.send_button.grid(row=1, column=2)

        tk.Label(m_prep, text="PAYLOAD:").grid(row=2, column=0)
        tk.Entry(
                m_prep,width=30,textvariable=self.payload_to_send
                ).grid(row=2, column=1)
        self.wrap_button = tk.Button(
                m_prep,
                text="WRAP PAYLOAD",
                command=self.sendPayload
                )
        self.wrap_button.grid(row=2, column=2)
        self.quicksend = tk.IntVar()
        tk.Checkbutton(
                m_prep, text="QUICK SEND (send message immediately upon wrapping)",
                variable=self.quicksend
                ).grid(row=3,column = 1)

        #ANALYSIS PANEL:
        tk.Label(analysis_panel, text="MESSAGE ANALYSIS").grid(row=0, column=0)
        self.readout = tk.Text(analysis_panel,width=50,height=15)
        self.readout.grid(row=1, column=0)		

        #NEG_TOOLS_PANEL:
        neg_tools_panel = tk.Frame(analysis_panel,padx=10,pady=10)		
        neg_tools_panel.grid(row=4,column=0)
        tk.Label(
                neg_tools_panel,
                text="NEGOTIATION TOOLS"
                ).grid(row=0, column=0, columnspan=4)
        tk.Label(neg_tools_panel, text="hash").grid(row=1, column=0)
        tk.Label(neg_tools_panel, text="secret").grid(row=1, column=1)
        tk.Label(neg_tools_panel, text="B").grid(row=1, column=2)
        tk.Label(neg_tools_panel, text="parity").grid(row=1, column=3)		

        self.hash = tk.StringVar()
        self.B = tk.StringVar()
        self.secret = tk.StringVar()
        self.parity = tk.StringVar()

        tk.Entry(
                neg_tools_panel, 
                width=10,
                textvariable=self.hash
                ).grid(row=3,column=0)
        tk.Entry(neg_tools_panel, textvariable=self.secret).grid(row=3,column=1)
        tk.Entry(neg_tools_panel, textvariable=self.B).grid(row=3,column=2)
        tk.Entry(
                neg_tools_panel,
                width=10,
                textvariable=self.parity
                ).grid(row=3,column=3)
        tk.Button(
                neg_tools_panel,
                text="calculate hash",
                command=self.calculateHash
                ).grid(row=4,column=0,columnspan=2)		
        tk.Button(
                neg_tools_panel,
                text="calculate parity",
                command=self.calculateParity
                ).grid(row=5,column=1,columnspan=2)

        #AUTOPLAY_PANEL		
        tk.Label(autoplay_panel, text="PLAY").grid()
        self.ships_to_sink = [2,3,4,5] #used for losing autoplay
        self.next_row = 0
        self.next_col = 0
        self.autoplay = tk.StringVar()		
        self.autoplay.set("FAIR")

        tk.Radiobutton(
                autoplay_panel,
                text="FAIR",
                variable=self.autoplay,
                value="FAIR"
                ).grid(row=1, column=0)
        tk.Radiobutton(
                autoplay_panel,
                text="FIRST_MOVE",
                variable=self.autoplay,
                value="FIRST_MOVE"
                ).grid(row=2, column=0)
        tk.Radiobutton(
                autoplay_panel,
                text="SECOND_MOVE",
                variable=self.autoplay,
                value="SECOND_MOVE"
                ).grid(row=3, column=0)

        """
        #Autoplay is not functional, but maybe someday....


        self.state = tk.StringVar()		
        self.state.set("start")
        tk.Radiobutton(
            autoplay_panel, text="start", variable=self.state, value="start"
        ).grid(row=1, column=1)
        tk.Radiobutton(
            autoplay_panel,
            text="challenging",
            variable=self.state,
            value="challenging"
        ).grid(row=2, column=1)
        tk.Radiobutton(
            autoplay_panel,
            text="accepting",
            variable=self.state,
            value="accepting"
        ).grid(row=3, column=1)
        tk.Radiobutton(
            autoplay_panel,
            text="attacking",
            variable=self.state,
            value="attacking"
        ).grid(row=4, column=1)
        tk.Radiobutton(
            autoplay_panel,
            text="waiting",
            variable=self.state,
            value="waiting"
        ).grid(row=5, column=1)
        tk.Radiobutton(
            autoplay_panel,
            text="defending",
            variable=self.state,
            value="defending"
        ).grid(row=6, column=1)
"""

        tk.Button(
                autoplay_panel, text="send cha", command = self.sendCHA
                ).grid(row=1,column=2)
        tk.Button(
                autoplay_panel, text="send acc", command = self.sendACC
                ).grid(row=2,column=2)
        tk.Button(
                autoplay_panel, text="send rev", command = self.sendREV
                ).grid(row=3,column=2)
        tk.Button(
                autoplay_panel, text="send sho", command = self.sendSHO
                ).grid(row=4,column=2)
        tk.Button(
                autoplay_panel, text="send res", command = self.sendRES
                ).grid(row=5,column=2)

        #Generic state variables:
        self.ser_write_buffer =""
        self.ser_read_buffer = ""
        self.port = self.ser_port_box.get()
        self.last_shot = (0,0)

        try:
            self.ser = serial.Serial(
                    port=self.port, baudrate=baud, timeout=timeout
                    )
        except serial.SerialException:
            print("Visible ports: ",list(serial.tools.list_ports.comports()))
            self.ser = serial.Serial()
#			self.ser_port_box.delete(0,tk.END)


    def sendCHA(self):
        if not self.hash.get():
            self.calculateHash()
        self.sendSer(encode_message("CHA,"+self.hash.get()))

    def sendACC(self):
        #get best guess about challenger's secret parity:
        cracks = [parity(i) for i in crack_hash(int(self.hash.get()))]
        avg_parity = 1.0*sum(cracks)/len(cracks)
        self.disp("expected parity of opponent's secret is %d" %(avg_parity))
        if self.autoplay.get() == "FIRST_MOVE":
            self.B.set(avg_parity > 0.5)
        if self.autoplay.get() == "SECOND_MOVE":
            self.B.set(avg_parity < 0.5)
        if not self.B.get():
            self.B.set(random.randint(0,0xFFFF))
            self.disp("PyAgent generated random B (%s)" % self.B.get())		
        self.sendSer(encode_message("ACC,"+self.B.get()))

    def sendREV(self):
        if not self.secret.get():
            if not self.hash.get():
                self.calculateHash()
            else:
                self.secret.set(crack_hash(int(self.hash.get))[0])

        if not self.B.get():
            self.B.set("0")

        possible_secrets = crack_hash(int(self.hash.get()))
        tails = [i for i in possible_secrets 
                 if parity(i ^ int(self.B.get())) == 0]
        heads = [i for i in possible_secrets 
                 if parity(i ^ int(self.B.get())) == 1]
        if self.autoplay.get()=="SECOND_MOVE" and len(tails)>0:
            self.disp("PyAgent picked losing B")
            secret = tails[0]
        elif self.autoplay.get()=="FIRST_MOVE" and len(heads)>0:
            self.disp("PyAgent picked winning B")
            secret = heads[0]
        else:
            secret = self.secret.get()

        self.sendSer(encode_message("REV,"+str(secret)))
        self.calculateParity()

    def sendSHO(self):
        sho_mess = "SHO,%s,%s" %(self.next_row, self.next_col)
        self.next_row = random.randint(0,5)
        self.next_col = random.randint(0,9)
        #self.next_row +=1
        #if self.next_row==6:
        #	self.next_col+=1
        #	self.next_row=0			
        self.sendSer(encode_message(sho_mess))

    def sendRES(self):
        print(self.last_shot)
        print(self.ships_to_sink)
        if(self.autoplay.get()=="SECOND_MOVE"):
            res_mess = "RES,%s,%s,%d" % (
                    self.last_shot[0],self.last_shot[1],self.ships_to_sink[0]
                    )
            self.ships_to_sink = self.ships_to_sink[1:] + [self.ships_to_sink[0]]
        if(self.autoplay.get()=="FIRST_MOVE"):
            res_mess = "RES,%s,%s,%d" %(
                    self.last_shot[0],self.last_shot[1],0);	
        if(self.autoplay.get()=="FAIR"):
            res_mess = "RES,%s,%s,%d" %(
                    self.last_shot[0],self.last_shot[1],random.randint(0,1));	

        self.sendSer(encode_message(res_mess))


    def disp(self,string):
        self.readout.insert(tk.INSERT, "\n"+string)
        self.readout.see(tk.END)

    def reset(self):
        self.readout.delete("1.0",tk.END)		
        self.raw_serial_out.delete("1.0",tk.END)		
        self.raw_serial_in.delete("1.0",tk.END)			
        self.secret.set("")
        self.hash.set("")
        self.B.set("")
        self.parity.set("")
#		self.quicksend.set(False)
#		self.autoplay.set("FAIR")
#		self.state.set("START")
#		self.payload.delete(0,tk.END)			
#		self.raw_message_to_send.delete(0,tk.END)		


    def runSM(self,message_in):
        pass
#		if self.state == "start":
#			if re.match("$CHA", message_in):
#				self.B = random.randint(0xFFFF)
#				self.sendSer(encode_message("$ACC,%d"%(self.B)))
                #self.state = "accepting"

#		if re.search("SHO",message_in):
#			print("\nReceived SHO")
#			payload=message.split(",")
#						

#							
#			self.raw_message_to_send.set(encode_message(sho_mess))
#			self.sendSer()



    def decodeCharIn(self, char_in):
        if char_in == "\n":
            message = self.ser_read_buffer
            analysis = self.analyze_message(message)
            print("\n"+analysis)
            self.runSM(message)
            self.readout.insert(
                    tk.INSERT,
                    "\n\nRECEIVED MESSAGE: '%s'\n%s"%(message,analysis)
                    )
            self.readout.see(tk.END)
            self.ser_read_buffer = ""
        else:
            self.ser_read_buffer+= char_in

    def calculateParity(self):
        A = int(self.secret.get())
        B = int(self.B.get())
        self.parity.set(parity(A ^ B))	

    def calculateHash(self):
        if not self.secret.get():
            self.secret.set(random.randint(0,0xFFFF))
            self.readout.insert(
                    tk.INSERT, 
                    "\nPyAgent Generated random secret (%s)"%(self.secret.get())
                    )
        self.hash.set(beefHash(int(self.secret.get())))

    def analyze_message(self,message):
        """This updates the analysis window.
        This function is agnostic about who sent/received messages!"""
        message=message.rstrip("\n")
        ret="----message analysis------\nMESSAGE = '%s'"%(message)
        #first, get payload and checksum:
        match=re.match("\$(.*)\*[A-F0-9][A-F0-9]", message)
        if not match:

            ret += "\nDoes not appear to be valid NMEA format"
            return ret
        #now calculate payload:
        payload = match.group(1)
        if encode_message(payload) != message:
            ret += "\nPayload and checksum do not match"
            ret += "\n   Should be: " + encode_message(payload)

        #ok, now we can analyze individual messages:
        payload = payload.split(",")
        if payload[0]=="CHA":
            self.hash.set(int(payload[1]))
            if not student_release_mode:
                ret+= "\nValid secrets for this hash are: "

                ret+= ",".join([
                    " %d (%d)" % (i,parity(i)) for i in crack_hash(
                        int(self.hash.get())
                    )
                ])
        elif payload[0]=="ACC":
            self.B.set(int(payload[1]))
            ret+= "\nParity = %d" %(parity(int(self.B.get())))
        elif payload[0]=="REV":
            self.secret.set(payload[1])
            self.calculateParity()		
        elif payload[0]=="SHO":
            self.last_shot = (int(payload[1]), int(payload[2]))
        elif payload[0]=="RES":
            pass
        else:
            ret+= "\nDoes not appear to have a valid message type"
        return ret+"\n------------------------"


    def sendSer(self, message):
        """Send a message"""
        # There may or may not be a \n at the end of the input arg.
        message = message.rstrip("\n") + "\n"
        self.ser_write_buffer = self.ser_write_buffer + message

        #analyze message:		
        analysis = self.analyze_message(message)
        print(analysis)
        self.readout.insert(tk.INSERT,
                            """\n\nSENT: "%s\n%s""" %(message, analysis))
        self.readout.see(tk.END)

    def sendRaw(self):
        message = self.raw_message_to_send.get()
        self.sendSer(message)

    def sendPayload(self):
        #basically, just move the wrap
        payload = self.payload_to_send.get().upper()
        message = encode_message(payload)
        self.raw_message_to_send.set(message)
        if self.quicksend.get():
            self.sendRaw()

    def updateSer(self):		
        if not self.ser.is_open:
            self.serial_status_label.configure(text="SERIAL CLOSED")
            self.master.update()
            return

        #next, handle incoming chars:
        self.serial_status_label.configure(text="SERIAL OPEN")
        if (self.ser.in_waiting>0):
            serIn = self.ser.read(1).decode("ascii")[0]
            self.decodeCharIn(serIn)
            if(serIn == "\n"):
                serIn = "\\n\n"
                self.raw_serial_out.insert(tk.INSERT,"\n")
            elif (serIn< " " or serIn > "~"):
                serIn = "(0x%02X)"%(ord(serIn))
            self.raw_serial_in.insert(tk.INSERT,serIn)
            print(serIn,end="")

            self.raw_serial_out.see(tk.END) 
            self.raw_serial_in.see(tk.END) 

        #and now, outgoing chars:
        if len(self.ser_write_buffer) >0:
            serOut = self.ser_write_buffer[0]
            self.ser_write_buffer = self.ser_write_buffer[1:]
            #first, send whatever we got:
            self.ser.write(bytes(serOut, 'utf-8'))

            #and now print it on our screen
            if(serOut == "\n"):
                serOut = "\\n\n"
                self.raw_serial_in.insert(tk.INSERT, "\n")
            self.raw_serial_out.insert(tk.INSERT, serOut)

            self.raw_serial_out.see(tk.END) 
            self.raw_serial_in.see(tk.END) 

    def openSer(self):
        print("opening serial...")
        self.port = self.ser_port_box.get()
        self.ser = serial.Serial(port=self.port, baudrate=baud, timeout=timeout)

    def closeSer(self):
        try:
            self.ser.close()
            print("Closed serial...")
        except AttributeError:
            pass
        except serial.SerialException as e:
            print(e)

    def refresh(self):
        try:
            sys.stdout.flush()
            self.updateSer()
            root.after(refresh_period, self.refresh)
        except Exception as e:
            print(e)
            raise(e)

    def show_help(self):
        #		self.hide()
        otherFrame = tk.Toplevel()
#        otherFrame.geometry("400x300")
        otherFrame.title("agent.py Help")
        self.help_text_element = tk.Text(otherFrame)
        self.help_text_element.insert(tk.END, help_text)
        self.help_text_element.grid(row=0, column=0)
        self.help_scroller = tk.Scrollbar(otherFrame, 
                                          command=self.help_text_element.yview)
        self.help_scroller.grid(row=0, column=1, sticky='nsew')

#        handler = lambda: self.onCloseOtherFrame(otherFrame)
#        btn = Tk.Button(otherFrame, text="Close", command=handler)
#        btn.pack()




#%%
if __name__=="__main__":
    print("starting...")

    sys.stdout.flush()
    root = tk.Tk()
#	root.after(10000, lambda: root.destroy()) #add an autoclose to keep these
#	things from cluttering up my desktop


    pyAgent = PyAgent(root)

    root.protocol("WM_DELETE_WINDOW", pyAgent.closeSer)
    root.protocol("WM_DELETE_WINDOW", lambda: root.destroy())
    root.after(100, pyAgent.refresh)
    root.mainloop()
