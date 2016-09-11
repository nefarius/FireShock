using System;

namespace FireShockCockpit
{
    public class FireShockDetectorEventArgs : EventArgs
    {
        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="classguid"></param>
        /// <param name="name"></param>
        public FireShockDetectorEventArgs(string classguid, string name)
        {
            this.ClassGuid = classguid;
            this.Name = name;
        }


        /// <summary>
        /// The GUID for the interface device class
        /// </summary>
        public string ClassGuid { get; internal set; }


        /// <summary>
        /// A null-terminated string that specifies the name of the device
        /// </summary>
        public string Name { get; internal set; }

    }
}
