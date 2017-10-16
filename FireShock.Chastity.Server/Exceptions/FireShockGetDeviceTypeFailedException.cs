using System;
using System.Runtime.Serialization;

namespace FireShock.Chastity.Server.Exceptions
{
    public class FireShockGetDeviceTypeFailedException : Exception
    {
        public FireShockGetDeviceTypeFailedException()
        {
        }

        public FireShockGetDeviceTypeFailedException(string message) : base(message)
        {
        }

        public FireShockGetDeviceTypeFailedException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected FireShockGetDeviceTypeFailedException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}