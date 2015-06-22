using System;
using System.IO.Ports;
using System.Net;
using System.Net.Sockets;
using System.Windows.Forms;
using System.Text;
using System.Threading;
using System.IO;
namespace Ax25TestApplication
{
    #region Gestion de la réception des packets
    internal delegate void PacketReceivedEventHandler(object sender, PacketReceivedEventArgs e);
    internal class PacketReceivedEventArgs : EventArgs
    {
        private byte[] pkt;

        internal PacketReceivedEventArgs(byte[] pkt)
        {
            this.pkt = pkt;
        }

        public byte[] Packet { get { return this.pkt; } }
    }
    #endregion

    class TNC
    {
        #region Constantes
        private static int BUFFER_SIZE = 512;
        private static int ADDRESS_SIZE = 7;
        #endregion

        #region Variables
        internal event PacketReceivedEventHandler PacketReceived;
        private SerialPort USBPort;
        private byte[] buffer = new byte[BUFFER_SIZE];
        private int pointer = 0;
        #endregion

        #region Contructeur et fonctions principales
        // Constructeur TNC
        public TNC(string Port)
        {
            // Crée une connection série
            USBPort = new SerialPort();

            // Configuration du port USB
            USBPort.PortName = Port;
            USBPort.BaudRate = 38400;

            // Pas de paritée
            USBPort.Parity = (Parity)Enum.Parse(typeof(Parity), "None");

            // Data codée sur 1 byte
            USBPort.DataBits = 8;

            // Un stop bit
            USBPort.StopBits = (StopBits)Enum.Parse(typeof(StopBits), "1");

            // Pas de gestion de flux
            USBPort.Handshake = (Handshake)Enum.Parse(typeof(Handshake), "None");

            // Configuration des Timeouts
            USBPort.ReadTimeout = 500;
            USBPort.WriteTimeout = 500;

            // Gestion de l'évènement DataReceived
            USBPort.DataReceived += new SerialDataReceivedEventHandler(usbDataReceived);

            // Connection : Ouverture du port
            USBPort.Open();
        }

        // Fonction isConnected return true si connected
        public bool IsConnected()
        {
            return USBPort.IsOpen;
        }

        // Reset du TNC
        public void Reset()
        {
            // Magic Sequence pour mode KISS OFF
            byte[] exitSeq = { 0xc0, 0xff, 0xc0 };
            USBPort.Write(exitSeq, 0, 3);

            // Reset général
            SendToTNC("%R");
        }
        
        // Reset puis fermeture du port série (USB) 
        public void Disconnect()
        {
            Reset();
            USBPort.Close();
            USBPort.Dispose();
        }

        // Ecriture des cmds de configuration du TNC
        private void SendToTNC(string data)
        {
            byte[] toSend = new byte[data.Length + 2];

            toSend[0] = 0x1b; // Fanion Start
            char[] c = data.ToCharArray();
            for (int i = 0; i < c.Length; i++)
            {
                toSend[i + 1] = (byte)c[i];
            }
            toSend[toSend.Length - 1] = 0x0d; // Fanion End

            // Ecriture sur le port
            USBPort.Write(toSend, 0, toSend.Length);

            // Tempo
            Thread.Sleep(data.Length * 8);
        }
        #endregion

        #region Tx
        // Configuration du TNC en PK9K6 (G3RUH)
        public void ConfigureInPacketRadio()
        {
            // Configuration du TNC
            SendToTNC("@D0");   // Half duplex
            SendToTNC("@F0");   // Don't send flags during pause
            SendToTNC("%B9600");// 9600 baud FSK
            SendToTNC("X1");    // PTT normal operations
            SendToTNC("%X400"); // Set TNC Output level to 400mV
 
            // Divers
            SendToTNC("%T25");  // Tx Delay (x10ms)
            SendToTNC("P128");  // Persistence
            SendToTNC("W20");   // Slot Time (ms)
            SendToTNC("O2");    // Max Frames
            SendToTNC("R0");    // Digipeating OFF

            // Desable Call check
            SendToTNC("@V0");   // Don't check CallSign

            // KISS Mode ON
            SendToTNC("@K");    // Set KISS mode ON
        }

        // Fonction send data en mode KISS
        public bool SendKISSData(byte[] data)
        {
            // Buffer : - 3 bytes pour 2 FEND et 1 PORT CMD
            //          - 2X plus de bytes en cas de byte stuffing
            byte[] toSend = new byte[data.Length * 2 + 3];

            // FEND byte
            toSend[0] = 0xc0;

            // PORT + COMMAND bits
            toSend[1] = 0x00;  // Port radio = 0, Cmd = Send Data = 0

            // Byte stuffing for 0xC0
            int counter = 2;

            // Byte Stuffing
            for (int i = 0; i < data.Length; i++)
            {
                if (data[i] == 0xc0) // On remplace 0xC0 par 0xDB 0xDC
                {
                    toSend[counter] = 0xdb;
                    toSend[counter + 1] = 0xdc;
                    counter += 2;
                }
                else if (data[i] == 0xdb) // On remplace par 0xDB 0xDD
                {
                    toSend[counter] = 0xdb;
                    toSend[counter + 1] = 0xdd;
                    counter += 2;
                }
                else
                {
                    toSend[counter] = data[i];
                    counter++;
                }
            }

            // FEND byte
            toSend[counter] = 0xc0;

            // Envois de la trame KISS
            bool success = true;

            try
            {
                USBPort.Write(toSend, 0, counter + 1);
            }
            catch (InvalidOperationException io_exc)
            {
                success = false;
            }
            catch (UnauthorizedAccessException ua_exc)
            {
                success = false;
            }

            // Tempo
            Thread.Sleep((toSend.Length * 8 + 32) / 2);

            return success;
        }
        #endregion

        #region Rx
        // Réception des packets KISS
        private void usbDataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                while (this.USBPort.BytesToRead != 0)
                {
                    this.buffer[this.pointer] = (byte)this.USBPort.ReadByte();
                    if (this.buffer[this.pointer] == 0xC0)
                    {
                        byte[] command = new byte[this.pointer + 1];
                        Array.Copy(this.buffer, command, this.pointer + 1);
                        if (analyzeCommand(command, out command))
                        {
                            if (this.PacketReceived != null)
                            {
                                this.PacketReceived(this, new PacketReceivedEventArgs(command));
                            }
                        }

                        //Resets for next command
                        this.buffer = new byte[BUFFER_SIZE];
                        this.pointer = 0;
                    }
                    else
                    {
                        this.pointer++;
                    }
                }
            }
            catch (InvalidOperationException ex)
            {
                this.USBPort.Close();
            }
        }

        // Suppression du Byte Stuffing
        private byte[] removeEscapes(byte[] buf)
        {
            int count = 0;
            foreach (byte b in buf)
                if (b == 0xDB) ++count;

            byte[] toReturn = new byte[buf.Length - count];

            int i = 0;
            int j = 0;
            for (; i < buf.Length; i++, j++)
            {
                if (buf[i] == 0xDB)
                {
                    ++i;
                    if (buf[i] == 0xDC)
                        buf[i] = 0xC0;
                    if (buf[i] == 0xDD)
                        buf[i] = 0xDB;
                }
                toReturn[j] = buf[i];
            }
            return toReturn;
        }

        // Décodage de la trame AX.25 dans un .txt (mode debug)
        private bool analyzeCommand(byte[] buf, out byte[] toRet)
        {
            int iterator = 0;

            buf = removeEscapes(buf);

            if (buf.Length > 1 && buf[0] == 0x00 && buf[buf.Length - 1] == 0xC0)
            {
                StreamWriter fichierTxt = new StreamWriter("Test.txt");
                fichierTxt.WriteLine("Frame received at " + DateTime.Now.ToLongTimeString() + " :");
                fichierTxt.WriteLine();

                ++iterator;

                fichierTxt.Write("SRC : ");
                for (int i = 0; i < ADDRESS_SIZE; i++)
                {
                    fichierTxt.Write("0x" + buf[iterator + i].ToString("X") + " ");
                }
                fichierTxt.WriteLine();
                fichierTxt.WriteLine();

                iterator += ADDRESS_SIZE;

                fichierTxt.Write("DEST : ");
                for (int i = 0; i < ADDRESS_SIZE; i++)
                {
                    fichierTxt.Write("0x" + buf[iterator + i].ToString("X") + " ");
                }
                fichierTxt.WriteLine();
                fichierTxt.WriteLine();

                iterator += ADDRESS_SIZE;

                fichierTxt.WriteLine("CTRL: " + "0x" + buf[iterator++].ToString("X"));
                fichierTxt.WriteLine();

                fichierTxt.WriteLine("PID: " + "0x" + buf[iterator++].ToString("X"));
                fichierTxt.WriteLine();

                fichierTxt.WriteLine("INFO: ");
                for (int i = 0; iterator + i < buf.Length - 1; i++)
                {
                    fichierTxt.Write("0x" + buf[iterator + i].ToString("X") + " ");
                }
                fichierTxt.WriteLine();
                fichierTxt.WriteLine();

                iterator += buf.Length - iterator - 1;

                fichierTxt.Close();
                toRet = new byte[iterator - 1];
                Array.Copy(buf, 1, toRet, 0, iterator - 1);
                return true;
            }
            else if (buf.Length == 1 && buf[0] == 0xC0)
            {
                toRet = null;
                return false;
            }
            else
            {
                toRet = null;
                return false;
            }
        }
        #endregion
    }
}
