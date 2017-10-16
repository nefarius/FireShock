using System;
using System.Runtime.Serialization;

namespace FireShock.Chastity.Server.Exceptions
{
    public class FireShockSetHostBdAddrFailedException : Exception
    {
        public FireShockSetHostBdAddrFailedException()
        {
        }

        public FireShockSetHostBdAddrFailedException(string message) : base(message)
        {
        }

        public FireShockSetHostBdAddrFailedException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected FireShockSetHostBdAddrFailedException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}