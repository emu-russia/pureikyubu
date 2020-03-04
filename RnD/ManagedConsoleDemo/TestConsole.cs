using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ManagedConsoleDemo
{
    class TestConsole : ManagedConsole.Console
    {
        public override void OnKeyInput(KeyInfo key)
        {
            PrintAt(ManagedConsole.ColorCode.NORM, 10, 10, "                             ");
            PrintAt(ManagedConsole.ColorCode.NORM, 10, 10, key.KeyCode.ToString());
            Invalidate();

            base.OnKeyInput(key);
        }
    }

}
