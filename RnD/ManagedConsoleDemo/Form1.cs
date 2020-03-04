using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using ManagedConsole;

namespace ManagedConsoleDemo
{
    public partial class Form1 : Form
    {
        TestConsole con = new TestConsole();

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
            con.Allocate("Demo", new Size(80, 25));
        }

        private void closeCosoleToolStripMenuItem_Click(object sender, EventArgs e)
        {
            con.Free();
        }

        private void fillTestToolStripMenuItem_Click(object sender, EventArgs e)
        {
            con.Fill(new Rectangle(1, 1, 10, 10), ColorCode.DBLUE);
            con.Invalidate();
        }

        private void printTestToolStripMenuItem_Click(object sender, EventArgs e)
        {
            con.PrintAt(ColorCode.RED, 5, 5, "Hello, world!\nNew\ttabbed\tline.");
            con.Invalidate();
        }

        private void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            con.Free();
        }

    }
}
