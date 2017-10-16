using System;
using System.Runtime.Serialization;

namespace FireShock.Chastity.Server.Exceptions
{
    public class FireShockWriteOutputReportFailedException : Exception
    {
        public FireShockWriteOutputReportFailedException()
        {
        }

        public FireShockWriteOutputReportFailedException(string message) : base(message)
        {
        }

        public FireShockWriteOutputReportFailedException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected FireShockWriteOutputReportFailedException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}