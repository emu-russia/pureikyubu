using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ManagedConsoleDemo
{
    public partial class Form1 : Form
    {
        ManagedConsole.Console con = new ManagedConsole.Console();

        public Form1()
        {
            InitializeComponent();
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void openConsoleToolStripMenuItem_Click(object sender, EventArgs e)
        {
            con.Allocate("Demo", new Size(80, 60));
        }

        private void closeCosoleToolStripMenuItem_Click(object sender, EventArgs e)
        {
            con.Free();
        }
    }
}
