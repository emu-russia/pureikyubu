// Control for selecting a time interval and displaying event statistics in the specified interval.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using System.Drawing.Drawing2D;

namespace EventLogMonitor
{
    public partial class TimeLapseControl: Control
    {
        private BufferedGraphics gfx = null;
        private BufferedGraphicsContext context;
        private List<EventLogEntry> dataModel = new List<EventLogEntry>();

        /// <summary>
        /// Timeline change event, in which events from the data model that are included in this timeline only will be sent
        /// </summary>
        /// <param name="sender">this control</param>
        /// <param name="timeLineEntries">events from the data model that are included in this timeline only</param>
        public delegate void TimeLapseChanged(object sender, EventArgs args);
        public event TimeLapseChanged OnTimeLapseChanged = null;

        /// <summary>
        /// Custom control properties
        /// </summary>

        public int SegmentsPerView { get; set; } = 10;
        public int ScrollBarHeightPrcs { get; set; } = 5;
        public int TimelineHeightPrcs { get; set; } = 5;
        public Color SegmentLineColor { get; set; } = Color.AliceBlue;
        public Color TimelineTextColor { get; set; } = Color.AliceBlue;
        public Color SelectedSegmentBackgroundColor { get; set; } = Color.Orange;
        public int SegmentDeltaSidePrc { get; set; } = 4;
        public int SegmentBorderWidth { get; set; } = 2;
        public Color SegmentBorderColor { get; set; } = Color.Firebrick;
        public Color SegmentColor { get; set; } = Color.DarkOrchid;
        public int ScrollBarDeltaXPrc { get; set; } = 1;
        public int ScrollBarDeltaYPrc { get; set; } = 4;
        public int ScrollBarDeltaCaretPrc { get; set; } = 10;
        public Color ScrollBarCaretColor { get; set; } = Color.DarkBlue;
        public int ScrollBarCaretBorderWidth { get; set; } = 2;
        public Color ScrollBarCaretBorderColor { get; set; } = Color.DarkCyan;
        public Color ScrollBarBackColor { get; set; } = Color.CadetBlue;

        public enum ScrollBarShape
        {
            Rect,
            Rounded,
        }

        public ScrollBarShape ScrollBarCaretShape { get; set; } = ScrollBarShape.Rect;

        public int ScrollBarCaretWidthPrc { get; set; } = 10;

        public TimeLapseControl()
        {
            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);
        }

        private void ReallocateGraphics()
        {
            context = BufferedGraphicsManager.Current;
            context.MaximumBuffer = new Size(Width + 1, Height + 1);

            gfx = context.Allocate(CreateGraphics(),
                 new Rectangle(0, 0, Width, Height));
        }

        private void DrawGrid (Graphics gr)
        {
            Pen dashed_pen = new Pen(SegmentLineColor, 1.5f);
            dashed_pen.DashStyle = DashStyle.Dot;

            int segments = SegmentsPerView;
            int segementWidth = this.Width / segments;
            int start_x = segementWidth;
            int start_y = TimelineHeightPrcs * this.Height / 100;
            int end_y = this.Height - ScrollBarHeightPrcs * this.Height / 100;

            while (segments > 1)
            {
                gr.DrawLine(dashed_pen, new Point(start_x, start_y), new Point(start_x, end_y));
                segments--;
                start_x += segementWidth;
            }
        }

        private void DrawTimeLine (Graphics gr)
        {
            int cutoffs = SegmentsPerView;
            int cutoffWidth = this.Width / cutoffs;
            int start_x = 0;

            while(cutoffs-- > 0)
            {
                string text = "00:11:22";

                gr.DrawString(text, this.Font, new SolidBrush(TimelineTextColor),
                    new Rectangle(start_x, 0, cutoffWidth, this.Height / 3));

                start_x += cutoffWidth;
            }
        }

        private void DrawSegments(Graphics gr)
        {
            int segments = SegmentsPerView;
            int segmentWidth = this.Width / SegmentsPerView;
            int segmentHeight = this.Height - (TimelineHeightPrcs + ScrollBarHeightPrcs) * this.Height / 100;
            int deltaSidePx = SegmentDeltaSidePrc * segmentWidth / 100;
            int start_x = 0;
            int start_y = TimelineHeightPrcs * this.Height / 100;

            // DEBUG
            Random rnd = new Random();

            while (segments-- > 0)
            {
                int y = rnd.Next(20, segmentHeight - 20);

                // DEBUG: Imitate selected segments

                bool selected = rnd.Next(0, 2) == 1;

                if (selected)
                {
                    gr.FillRectangle(new SolidBrush(SelectedSegmentBackgroundColor),
                        new Rectangle(start_x, start_y,
                        segmentWidth, segmentHeight));
                }

                // Body

                Rectangle rect = new Rectangle(start_x + deltaSidePx, start_y + y,
                        segmentWidth - 2 * deltaSidePx, segmentHeight - y);

                gr.FillRectangle(new SolidBrush(SegmentColor), rect);

                // Border

                Pen linePen = new Pen(SegmentBorderColor, SegmentBorderWidth);

                gr.DrawLine(linePen,
                    new Point(rect.X, rect.Y + rect.Height),
                    new Point(rect.X, rect.Y + 1));

                start_x += segmentWidth;
            }
        }

        private void DrawScrollBar(Graphics gr)
        {
            int sbHeight = ScrollBarHeightPrcs * this.Height / 100;
            int marginX = ScrollBarDeltaXPrc * this.Width / 100;
            int marginY = ScrollBarDeltaYPrc * sbHeight / 100;
            int stripeHeight = sbHeight - 2 * marginY;
            int stripeWidth = this.Width - 2 * marginX;
            int caretMargin = ScrollBarDeltaCaretPrc * stripeHeight / 100;
            int caretWidth = ScrollBarCaretWidthPrc * stripeWidth / 100;

            // Stripe

            Rectangle stripeRect = new Rectangle(marginX, this.Height - marginY - stripeHeight, stripeWidth, stripeHeight);
            gr.FillRectangle(new SolidBrush(ScrollBarBackColor), stripeRect);

            // Caret

            Random rnd = new Random();

            int caret_x = rnd.Next(10, this.Width - 2 * caretWidth);
            Rectangle caretRect = new Rectangle(caret_x, stripeRect.Y + caretMargin, caretWidth, stripeHeight - 2 * caretMargin);
            Brush caretBrush = new SolidBrush(ScrollBarCaretColor);

            switch (ScrollBarCaretShape)
            {
                case ScrollBarShape.Rect:
                    gr.FillRectangle(caretBrush, caretRect);
                    break;

                case ScrollBarShape.Rounded:
                    GraphicsPath path = RoundedRect(caretRect, 2);
                    gr.FillPath(caretBrush, path);
                    break;
            }

        }

        // https://stackoverflow.com/questions/33853434/how-to-draw-a-rounded-rectangle-in-c-sharp

        private static GraphicsPath RoundedRect(Rectangle bounds, int radius)
        {
            int diameter = radius * 2;
            Size size = new Size(diameter, diameter);
            Rectangle arc = new Rectangle(bounds.Location, size);
            GraphicsPath path = new GraphicsPath();

            if (radius == 0)
            {
                path.AddRectangle(bounds);
                return path;
            }

            // top left arc  
            path.AddArc(arc, 180, 90);

            // top right arc  
            arc.X = bounds.Right - diameter;
            path.AddArc(arc, 270, 90);

            // bottom right arc  
            arc.Y = bounds.Bottom - diameter;
            path.AddArc(arc, 0, 90);

            // bottom left arc 
            arc.X = bounds.Left;
            path.AddArc(arc, 90, 90);

            path.CloseFigure();
            return path;
        }

        private void DrawSelf (Graphics gr)
        {
            gr.Clear(this.BackColor);
            DrawSegments(gr);
            DrawTimeLine(gr);
            DrawGrid(gr);
            DrawScrollBar(gr);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            if (gfx == null)
                ReallocateGraphics();

            DrawSelf(gfx.Graphics);

            gfx.Render(e.Graphics);
        }

        protected override void OnSizeChanged(EventArgs e)
        {
            if (gfx != null)
            {
                gfx.Dispose();
                ReallocateGraphics();
            }

            Invalidate();
            base.OnSizeChanged(e);
        }

        public Keys GetModifierKeys()
        {
            return ModifierKeys;
        }

        public void AssignModel (List<EventLogEntry> model)
        {
            dataModel = model;
        }

    }
}
