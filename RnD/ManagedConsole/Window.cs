// Console windows interface

using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;

namespace ManagedConsole
{
    partial class Console
    {
        public class Window
        {
            Console parent;
            Rectangle winrect;
            bool active = false;

            // Create a window at the specified coordinates and specified size
            public Window(Console parent, Rectangle rect)
            {
                this.parent = parent;
                winrect = new Rectangle(rect.X, rect.Y, rect.Width, rect.Height);
                parent.windows.Add(this);
            }

            // Destroy window
            public void Destroy()
            {
                parent.windows.Remove(this);
            }

            // Refresh whole window contents immediately
            public void Invalidate()
            {
                parent.Blit(winrect);
            }

            // Activate or deactivate the window. Only the active window receives input events
            public void Activate(bool active)
            {
                this.active = active;
            }

            // Check if the window is currently active.
            public bool IsActive()
            {
                return active;
            }

            // Key Press Event
            public virtual void OnKeyInput(KeyInfo key)
            { }

            // Display a string of the specified color at the specified coordinates of the window.
            // Window contents is not updated immediately (Invalidate required).
            public void PrintAt(ColorCode color, int x, int y, string text)
            {
                parent.PrintAt(color, winrect.X + x, winrect.Y + y, text);
            }

            // Fill window background
            // Window contents is not updated immediately (Invalidate required).
            public void Fill(Rectangle rect, ColorCode backColor)
            {
                if (rect.X >= winrect.Width || rect.Y >= winrect.Height)
                    return;

                Rectangle worldRect = new Rectangle();

                worldRect.X = winrect.X + rect.X;
                worldRect.Y = winrect.Y + rect.Y;
                worldRect.Width = Math.Min(winrect.Width - rect.X, rect.Width);
                worldRect.Height = Math.Min(winrect.Height - rect.Y, rect.Height);

                parent.Fill(worldRect, backColor);
            }

        }

    }
}
