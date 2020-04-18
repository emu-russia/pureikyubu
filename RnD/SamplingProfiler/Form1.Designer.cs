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
            this.helpToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.aboutToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.addSampleDataToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.clearSampleDataToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.loadMainMemoryDumpToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.addSymbolsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            this.exitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.menuStrip1.SuspendLayout();
            this.SuspendLayout();
            // 
            // menuStrip1
            // 
            this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
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
            this.aboutToolStripMenuItem.Size = new System.Drawing.Size(180, 22);
            this.aboutToolStripMenuItem.Text = "About";
            this.aboutToolStripMenuItem.Click += new System.EventHandler(this.aboutToolStripMenuItem_Click);
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
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(635, 372);
            this.Controls.Add(this.menuStrip1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MainMenuStrip = this.menuStrip1;
            this.Name = "Form1";
            this.Text = "Sampling Profiler Inspector";
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
    }
}

