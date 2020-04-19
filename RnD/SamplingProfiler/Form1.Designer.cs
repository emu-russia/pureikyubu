namespace SamplingProfiler
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.menuStrip1 = new System.Windows.Forms.MenuStrip();
            this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.addSampleDataToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.clearSampleDataToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.loadMainMemoryDumpToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.addSymbolsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            this.exitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.helpToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.aboutToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.debugToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.checkVersionToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openFileDialogMap = new System.Windows.Forms.OpenFileDialog();
            this.openFileDialogJson = new System.Windows.Forms.OpenFileDialog();
            this.dumpSamplesToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openFileDialogBin = new System.Windows.Forms.OpenFileDialog();
            this.listView1 = new System.Windows.Forms.ListView();
            this.columnHeader1 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.columnHeader2 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.columnHeader3 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.menuStrip1.SuspendLayout();
            this.SuspendLayout();
            // 
            // menuStrip1
            // 
            this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.debugToolStripMenuItem,
            this.helpToolStripMenuItem});
            this.menuStrip1.Location = new System.Drawing.Point(0, 0);
            this.menuStrip1.Name = "menuStrip1";
            this.menuStrip1.Size = new System.Drawing.Size(635, 24);
            this.menuStrip1.TabIndex = 0;
            this.menuStrip1.Text = "menuStrip1";
            // 
            // fileToolStripMenuItem
            // 
            this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.addSampleDataToolStripMenuItem,
            this.clearSampleDataToolStripMenuItem,
            this.toolStripSeparator1,
            this.loadMainMemoryDumpToolStripMenuItem,
            this.addSymbolsToolStripMenuItem,
            this.toolStripSeparator2,
            this.exitToolStripMenuItem});
            this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
            this.fileToolStripMenuItem.Size = new System.Drawing.Size(37, 20);
            this.fileToolStripMenuItem.Text = "File";
            // 
            // addSampleDataToolStripMenuItem
            // 
            this.addSampleDataToolStripMenuItem.Name = "addSampleDataToolStripMenuItem";
            this.addSampleDataToolStripMenuItem.Size = new System.Drawing.Size(222, 22);
            this.addSampleDataToolStripMenuItem.Text = "Add sample data...";
            this.addSampleDataToolStripMenuItem.Click += new System.EventHandler(this.addSampleDataToolStripMenuItem_Click);
            // 
            // clearSampleDataToolStripMenuItem
            // 
            this.clearSampleDataToolStripMenuItem.Name = "clearSampleDataToolStripMenuItem";
            this.clearSampleDataToolStripMenuItem.Size = new System.Drawing.Size(222, 22);
            this.clearSampleDataToolStripMenuItem.Text = "Clear sample data";
            this.clearSampleDataToolStripMenuItem.Click += new System.EventHandler(this.clearSampleDataToolStripMenuItem_Click);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(219, 6);
            // 
            // loadMainMemoryDumpToolStripMenuItem
            // 
            this.loadMainMemoryDumpToolStripMenuItem.Name = "loadMainMemoryDumpToolStripMenuItem";
            this.loadMainMemoryDumpToolStripMenuItem.Size = new System.Drawing.Size(222, 22);
            this.loadMainMemoryDumpToolStripMenuItem.Text = "Load main memory dump...";
            this.loadMainMemoryDumpToolStripMenuItem.Click += new System.EventHandler(this.loadMainMemoryDumpToolStripMenuItem_Click);
            // 
            // addSymbolsToolStripMenuItem
            // 
            this.addSymbolsToolStripMenuItem.Name = "addSymbolsToolStripMenuItem";
            this.addSymbolsToolStripMenuItem.Size = new System.Drawing.Size(222, 22);
            this.addSymbolsToolStripMenuItem.Text = "Add symbols...";
            this.addSymbolsToolStripMenuItem.Click += new System.EventHandler(this.addSymbolsToolStripMenuItem_Click);
            // 
            // toolStripSeparator2
            // 
            this.toolStripSeparator2.Name = "toolStripSeparator2";
            this.toolStripSeparator2.Size = new System.Drawing.Size(219, 6);
            // 
            // exitToolStripMenuItem
            // 
            this.exitToolStripMenuItem.Name = "exitToolStripMenuItem";
            this.exitToolStripMenuItem.Size = new System.Drawing.Size(222, 22);
            this.exitToolStripMenuItem.Text = "Exit";
            this.exitToolStripMenuItem.Click += new System.EventHandler(this.exitToolStripMenuItem_Click);
            // 
            // helpToolStripMenuItem
            // 
            this.helpToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.aboutToolStripMenuItem});
            this.helpToolStripMenuItem.Name = "helpToolStripMenuItem";
            this.helpToolStripMenuItem.Size = new System.Drawing.Size(44, 20);
            this.helpToolStripMenuItem.Text = "Help";
            // 
            // aboutToolStripMenuItem
            // 
            this.aboutToolStripMenuItem.Name = "aboutToolStripMenuItem";
            this.aboutToolStripMenuItem.Size = new System.Drawing.Size(107, 22);
            this.aboutToolStripMenuItem.Text = "About";
            this.aboutToolStripMenuItem.Click += new System.EventHandler(this.aboutToolStripMenuItem_Click);
            // 
            // debugToolStripMenuItem
            // 
            this.debugToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.checkVersionToolStripMenuItem,
            this.dumpSamplesToolStripMenuItem});
            this.debugToolStripMenuItem.Name = "debugToolStripMenuItem";
            this.debugToolStripMenuItem.Size = new System.Drawing.Size(54, 20);
            this.debugToolStripMenuItem.Text = "Debug";
            // 
            // checkVersionToolStripMenuItem
            // 
            this.checkVersionToolStripMenuItem.Name = "checkVersionToolStripMenuItem";
            this.checkVersionToolStripMenuItem.Size = new System.Drawing.Size(180, 22);
            this.checkVersionToolStripMenuItem.Text = "Check Version";
            this.checkVersionToolStripMenuItem.Click += new System.EventHandler(this.checkVersionToolStripMenuItem_Click);
            // 
            // openFileDialogMap
            // 
            this.openFileDialogMap.DefaultExt = "map";
            this.openFileDialogMap.Filter = "Map files|*.map|All files|*.*";
            // 
            // openFileDialogJson
            // 
            this.openFileDialogJson.DefaultExt = "json";
            this.openFileDialogJson.Filter = "JSON files|*.json|All files|*.*";
            // 
            // dumpSamplesToolStripMenuItem
            // 
            this.dumpSamplesToolStripMenuItem.Name = "dumpSamplesToolStripMenuItem";
            this.dumpSamplesToolStripMenuItem.Size = new System.Drawing.Size(180, 22);
            this.dumpSamplesToolStripMenuItem.Text = "Dump Samples";
            this.dumpSamplesToolStripMenuItem.Click += new System.EventHandler(this.dumpSamplesToolStripMenuItem_Click);
            // 
            // openFileDialogBin
            // 
            this.openFileDialogBin.DefaultExt = "bin";
            this.openFileDialogBin.Filter = "Binary files|*.bin|All files|*.*";
            // 
            // listView1
            // 
            this.listView1.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader1,
            this.columnHeader2,
            this.columnHeader3});
            this.listView1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.listView1.FullRowSelect = true;
            this.listView1.HideSelection = false;
            this.listView1.Location = new System.Drawing.Point(0, 24);
            this.listView1.Name = "listView1";
            this.listView1.Size = new System.Drawing.Size(635, 348);
            this.listView1.TabIndex = 1;
            this.listView1.UseCompatibleStateImageBehavior = false;
            this.listView1.View = System.Windows.Forms.View.Details;
            // 
            // columnHeader1
            // 
            this.columnHeader1.Text = "Address (name)";
            this.columnHeader1.Width = 278;
            // 
            // columnHeader2
            // 
            this.columnHeader2.Text = "Time spent";
            this.columnHeader2.Width = 116;
            // 
            // columnHeader3
            // 
            this.columnHeader3.Text = "Number of entries";
            this.columnHeader3.Width = 186;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(635, 372);
            this.Controls.Add(this.listView1);
            this.Controls.Add(this.menuStrip1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MainMenuStrip = this.menuStrip1;
            this.Name = "Form1";
            this.Text = "Sampling Profiler Inspector";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.Form1_FormClosed);
            this.menuStrip1.ResumeLayout(false);
            this.menuStrip1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.MenuStrip menuStrip1;
        private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem helpToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem aboutToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem addSampleDataToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem clearSampleDataToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripMenuItem loadMainMemoryDumpToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem addSymbolsToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
        private System.Windows.Forms.ToolStripMenuItem exitToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem debugToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem checkVersionToolStripMenuItem;
        private System.Windows.Forms.OpenFileDialog openFileDialogMap;
        private System.Windows.Forms.OpenFileDialog openFileDialogJson;
        private System.Windows.Forms.ToolStripMenuItem dumpSamplesToolStripMenuItem;
        private System.Windows.Forms.OpenFileDialog openFileDialogBin;
        private System.Windows.Forms.ListView listView1;
        private System.Windows.Forms.ColumnHeader columnHeader1;
        private System.Windows.Forms.ColumnHeader columnHeader2;
        private System.Windows.Forms.ColumnHeader columnHeader3;
    }
}

