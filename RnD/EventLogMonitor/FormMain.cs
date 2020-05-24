using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using System.IO;

namespace EventLogMonitor
{
    public partial class FormMain : Form
    {
        List<EventLogEntry> loadedLog;

        public FormMain()
        {
            InitializeComponent();
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void openEventLogToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if ( openFileDialogJson.ShowDialog() == DialogResult.OK)
            {
                string text = File.ReadAllText(openFileDialogJson.FileName, Encoding.UTF8);
                LoadEventLog(text);
            }
        }

        private void LoadEventLog(string text)
        {
            loadedLog = EventLog.ParseLog(text);

            // ResetTimeLapse();
            PopulateTree();
            UpdateListView();
        }

        private void PopulateTree ()
        {

        }

        private void UpdateListView ()
        {
            listView1.Items.Clear();

            listView1.BeginUpdate();

            foreach (var entry in loadedLog)
            {
                ListViewItem item = new ListViewItem(entry.timestamp.ToString());

                item.SubItems.Add(entry.subsystem.ToString());
                item.SubItems.Add(entry.type.ToString());
                item.SubItems.Add(Encoding.Default.GetString(entry.data));

                listView1.Items.Add(item);
            }

            listView1.EndUpdate();
        }

    }
}
