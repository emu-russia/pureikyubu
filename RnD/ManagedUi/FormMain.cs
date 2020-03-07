using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ManagedUi
{
    public partial class FormMain : Form
    {
        public FormMain()
        {
            InitializeComponent();
        }

        private void FormMain_Load(object sender, EventArgs e)
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
    }
}
