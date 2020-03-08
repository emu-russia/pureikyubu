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

namespace ManagedUi
{
    public partial class FormMain : Form
    {
        bool aboutShown = false;
        Config config = new Config();

        public FormMain()
        {
            InitializeComponent();
        }

        /// <summary>
        /// set proper current working directory, create missing directories
        /// </summary>
        private void InitFileSystem ()
        {
            // set current working directory relative to Dolwin executable

            // make sure, that Dolwin has data directory
            if (!Directory.Exists(".\\Data"))
            {
                Directory.CreateDirectory(".\\Data");
            }
        }

        private void FormMain_Load(object sender, EventArgs e)
        {
            InitFileSystem();

            config.Load();

            Text = Properties.Resources.APPNAME + " - " + Properties.Resources.APPDESC + " (" + Properties.Resources.APPVER + ")";

            // DEBUG
            GameListDemo();
        }

        void GameListDemo()
        {
            var imageList = new ImageList();
            imageList.ImageSize = new Size(96, 32);
            imageList.Images.Add("icon1", Properties.Resources.padStart);

            listView1.LargeImageList = imageList;
            listView1.SmallImageList = imageList;

            var listViewItem = listView1.Items.Add("");
            listViewItem.ImageIndex = 0;
            listViewItem.ImageKey = "icon1";

            listViewItem.SubItems.Add("Game name");
            listViewItem.SubItems.Add("1.4 GB");
            listViewItem.SubItems.Add("XXXX-XX");
            listViewItem.SubItems.Add("Comments");
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void aboutDolwinToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (!aboutShown)
            {
                FormAbout about = new FormAbout();
                about.FormClosed += About_FormClosed;
                about.Show();
                aboutShown = true;
            }
        }

        private void About_FormClosed(object sender, FormClosedEventArgs e)
        {
            aboutShown = false;
        }

        private void testNativeDllToolStripMenuItem_Click(object sender, EventArgs e)
        {
            DolwinNativeDll.Test();
        }

        private void FormMain_FormClosing(object sender, FormClosingEventArgs e)
        {
            config.Save();
        }

    }
}
