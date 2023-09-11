﻿using System;
using System.Runtime.CompilerServices;

namespace Carrot {
    public static class Utilities {
        public static void TODO(string message = "Something was not implemented") {
            throw new NotImplementedException(message);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern int GetMaxComponentCount();
        
        public delegate T Action<out T>();
        public delegate void SimpleAction();

        public static T Profile<T>(string name, Action<T> a) {
            BeginProfilingZone(name);
            T result = a();
            EndProfilingZone(name);
            return result;
        }

        public static void Profile(string name, SimpleAction a) {
            BeginProfilingZone(name);
            a();
            EndProfilingZone(name);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void BeginProfilingZone(string name);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void EndProfilingZone(string name);
    }
}
