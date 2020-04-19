using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SamplingProfiler
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();

            Jdi.InitEmu();
        }

        private void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            Jdi.ShutdownEmu();
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void aboutToolStripMenuItem_Click(object sender, EventArgs e)
        {
            FormAbout about = new FormAbout();
            about.ShowDialog();
        }

        private void addSampleDataToolStripMenuItem_Click(object sender, EventArgs e)
        {

        }

        private void clearSampleDataToolStripMenuItem_Click(object sender, EventArgs e)
        {

        }

        private void loadMainMemoryDumpToolStripMenuItem_Click(object sender, EventArgs e)
        {

        }

        private void addSymbolsToolStripMenuItem_Click(object sender, EventArgs e)
        {

        }

        private void checkVersionToolStripMenuItem_Click(object sender, EventArgs e)
        {
            string reply = Jdi.CallJdi("GetVersion");
        }

    }
}
