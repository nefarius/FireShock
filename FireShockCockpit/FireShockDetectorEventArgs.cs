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
        public FireShockDetectorEventArgs(Guid classguid, string name)
        {
            this.ClassGuid = classguid;
            this.Name = name;
        }


        /// <summary>
        /// The GUID for the interface device class
        /// </summary>
        public Guid ClassGuid { get; internal set; }


        /// <summary>
        /// A null-terminated string that specifies the name of the device
        /// </summary>
        public string Name { get; internal set; }

        public override string ToString()
        {
            return Name;
        }
    }
}
