using System;
using System.Threading;
using System.Runtime.InteropServices;

namespace Mapper
{
    public enum Type
    {
        Device    = 0x01,             //!< Devices only.
        SignalIn  = 0x02,             //!< Input signals.
        SignalOut = 0x04,             //!< Output signals.
        Signal    = 0x06,             //!< All signals.
        MapIn     = 0x08,             //!< Incoming maps.
        MapOut    = 0x10,             //!< Outgoing maps.
        Map       = 0x18,             //!< All maps.
        Object    = 0x1F,             //!< All objects: devices, signale, and maps
        List      = 0x40,             //!< object query.
        Graph     = 0x41,             //!< Graph.
        Boolean   = 'b',  /* 0x62 */  //!< Boolean value.
        Type      = 'c',  /* 0x63 */  //!< libmapper data type.
        Double    = 'd',  /* 0x64 */  //!< 64-bit float.
        Float     = 'f',  /* 0x66 */  //!< 32-bit float.
        Int64     = 'h',  /* 0x68 */  //!< 64-bit integer.
        Int32     = 'i',  /* 0x69 */  //!< 32-bit integer.
        String    = 's',  /* 0x73 */  //!< String.
        Time      = 't',  /* 0x74 */  //!< 64-bit NTP timestamp.
        Pointer   = 'v',  /* 0x76 */  //!< pointer.
        Null      = 'N'   /* 0x4E */  //!< NULL value.
    }

    public enum Operator {
        DoesNotExist            = 0x01, //!< Property does not exist.
        IsEqual                 = 0x02, //!< Property value == query value.
        Exists                  = 0x03, //!< Property exists for this entity.
        IsGreaterThan           = 0x04, //!< Property value > query value.
        IsGreaterThanOrEqual    = 0x05, //!< Property value >= query value
        IsLessThan              = 0x06, //!< Property value < query value
        IsLessThanOrEqual       = 0x07, //!< Property value <= query value
        IsNotEqual              = 0x08, //!< Property value != query value
        All                     = 0x10, //!< Applies to all elements of value
        Any                     = 0x20  //!< Applies to any element of value
    }

    public enum Direction {
        Incoming    = 0x01, //!< Signal is an input
        Outgoing    = 0x02, //!< Signal is an output
        Any         = 0x03, //!< Either incoming or outgoing
        Both        = 0x04  /*!< Both directions apply.  Currently signals cannot be both inputs
                             *   and outputs, so this value is only used for querying device maps
                             *   that touch only local signals. */
    }

    public class Time
    {
        // TODO: port time implementation instead of using unsafe library calls
        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        unsafe private static extern int mpr_time_set_dbl(void* obj, double sec);
        unsafe public Time setDouble(Double seconds)
        {
            fixed (void* t = &_time) {
                mpr_time_set_dbl(t, seconds);
            }
            return this;
        }
        internal long _time;
    }

    public class List
    {
        public List() {}
        ~List() {}

        // cast to vector

        // Iterator functions

        // public getSize();
        // public join();
        // public intersect();
        // public filter();
        // public subtract();
    }

    public enum Property
    {
        Bundle              = 0x0100,
        Data                = 0x0200,
        Device              = 0x0300,
        Direction           = 0x0400,
        Ephemeral           = 0x0500,
        Expression          = 0x0600,
        Host                = 0x0700,
        Id                  = 0x0800,
        IsLocal             = 0x0900,
        Jitter              = 0x0A00,
        Length              = 0x0B00,
        LibVersion          = 0x0C00,
        Linked              = 0x0D00,
        Max                 = 0x0E00,
        Min                 = 0x0F00,
        Muted               = 0x1000,
        Name                = 0x1100,
        NumInstances        = 0x1200,
        NumMaps             = 0x1300,
        NumMapsIn           = 0x1400,
        NumMapsOut          = 0x1500,
        NumSigsIn           = 0x1600,
        NumSigsOut          = 0x1700,
        Ordinal             = 0x1800,
        Period              = 0x1900,
        Port                = 0x1A00,
        ProcessingLocation  = 0x1B00,
        Protocol            = 0x1C00,
        Rate                = 0x1D00,
        Scope               = 0x1E00,
        Signal              = 0x1F00,
        Status              = 0x2100,
        StealingMode        = 0x2200,
        Synced              = 0x2300,
        Type                = 0x2400,
        Unit                = 0x2500,
        UseInstances        = 0x2600,
        Version             = 0x2700
    }

    public abstract class Object
    {
        public enum Status {
            Expired      = 0x01,
            Staged       = 0x02,
            Ready        = 0x3E,
            Active       = 0x7E,
            Reserved     = 0x80,
            Any          = 0xFF
        }

        public class PropVal
        {
            internal PropVal(int prop, string key, dynamic value)
            {
                _prop = prop;
                _key = key;
                _value = value;
            }

            public dynamic getValue()
            {
                return _value;
            }

            internal int _prop;
            internal string _key;
            internal int _publish = 0;
            internal dynamic _value;
        }

        protected Object() {}
        protected Object(IntPtr obj)
            { _obj = obj; }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern int mpr_obj_get_type(IntPtr obj);
        public Type getType()
            { return (Type) mpr_obj_get_type(_obj); }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern int mpr_obj_get_num_props(IntPtr obj);
        public int getNumProperties()
            { return mpr_obj_get_num_props(_obj); }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        unsafe internal static extern int mpr_obj_get_prop_by_idx(IntPtr obj, int idx, ref char *key,
                                                                  ref int len, ref int type,
                                                                  ref void *value, ref int publish);

        unsafe internal PropVal _getProperty<P>(P prop)
        {
            char *key = null;
            int len = 0;
            int type = 0;
            void *val = null;
            int pub = 0;
            int idx;
            switch (prop) {
                case int i:
                    idx = mpr_obj_get_prop_by_idx(this._obj, i, ref key, ref len,
                                                  ref type, ref val, ref pub);
                    break;
                case Property p:
                    idx = mpr_obj_get_prop_by_idx(this._obj, (int)p, ref key, ref len,
                                                  ref type, ref val, ref pub);
                    break;
                default:
                    Console.WriteLine("error: unhandled property type in getProperty().");
                    return null;
            }
            Console.WriteLine("check3 len");
            if (0 == idx || 0 == len)
                return null;
            Console.WriteLine("making new PropVal");
            PropVal pv = new PropVal(idx, new string(key), buildValue(len, type, val));
            Console.WriteLine("made new PropVal");
            return pv;
        }
        public PropVal getProperty(int prop)
        {
            return _getProperty<int>(prop);
        }
        public PropVal getProperty(Property prop)
        {
            return _getProperty<Property>(prop);
        }

        unsafe internal dynamic buildValue(int len, int type, void *value)
        {
            Console.WriteLine("buildValue");
            if (0 == len)
                return null;
            switch (type) {
                case (int)Type.Int32:
                    if (1 == len)
                        return *(int*)value;
                    else {
                        int[] arr = new int[len];
                        Marshal.Copy(new IntPtr(value), arr, 0, len);
                        return arr;
                    }
                case (int)Type.Float:
                    if (1 == len)
                        return *(float*)value;
                    else {
                        float[] arr = new float[len];
                        Marshal.Copy(new IntPtr(value), arr, 0, len);
                        return arr;
                    }
                case (int)Type.Double:
                    if (1 == len)
                        return *(double*)value;
                    else {
                        double[] arr = new double[len];
                        Marshal.Copy(new IntPtr(value), arr, 0, len);
                        return arr;
                    }
                case (int)Type.String:
                    Console.WriteLine("error: string array properties not currently supported by C# bindings.");
                    return null;
                default:
                    Console.WriteLine("error: unhandled data type in Object.buildValue().");
                    return null;
            }
        }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        unsafe internal static extern int mpr_obj_get_prop_by_key(IntPtr obj,
                                                                  [MarshalAs(UnmanagedType.LPStr)] string key,
                                                                  ref int len, ref int type,
                                                                  ref void *value, ref int publish);
        unsafe public dynamic getProperty(string key)
        {
            Console.WriteLine("Object.getProperty(string)");
            int len = 0;
            int type = 0;
            void *val = null;
            int pub = 0;
            int idx;
            idx = mpr_obj_get_prop_by_key(this._obj, key, ref len, ref type, ref val, ref pub);
            Console.WriteLine("check1, idx=" + idx + ", len=" + len);
            if (0 == idx || 0 == len)
                return null;
            return buildValue(len, type, val);
        }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        unsafe internal static extern
        int mpr_obj_get_prop_as_int32(IntPtr obj, int prop, [MarshalAs(UnmanagedType.LPStr)] string key);

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        unsafe internal static extern
        int mpr_obj_set_prop(IntPtr obj, int prop, [MarshalAs(UnmanagedType.LPStr)] string key,
                             int len, int type, void* value, int publish);
        unsafe public Object setProperty<P, T>(P prop, T value)
        {
            int _prop = 0;
            string _key = null;

            switch (prop) {
                case Property p:    _prop = (int)p; break;
                case string s:      _key = s;       break;
                default:
                    Console.WriteLine("unknown property specifier in method setProperty().");
                    return this;
            }

            switch (value) {
                case int i:
                    mpr_obj_set_prop(_obj, _prop, _key, 1, (int)Type.Int32, (void*)&i, 1);
                    break;
                case float f:
                    mpr_obj_set_prop(_obj, _prop, _key, 1, (int)Type.Float, (void*)&f, 1);
                    break;
                case double d:
                    mpr_obj_set_prop(_obj, _prop, _key, 1, (int)Type.Double, (void*)&d, 1);
                    break;
                case int[] i:
                    fixed(int *temp = &i[0]) {
                        IntPtr intPtr = new IntPtr((void*)temp);
                        mpr_obj_set_prop(_obj, _prop, _key, i.Length, (int)Type.Int32, (void*)intPtr, 1);
                    }
                    break;
                case float[] f:
                    fixed(float *temp = &f[0]) {
                        IntPtr intPtr = new IntPtr((void*)temp);
                        mpr_obj_set_prop(_obj, _prop, _key, f.Length, (int)Type.Float, (void*)intPtr, 1);
                    }
                    break;
                case double[] d:
                    fixed(double *temp = &d[0]) {
                        IntPtr intPtr = new IntPtr((void*)temp);
                        mpr_obj_set_prop(_obj, _prop, _key, d.Length, (int)Type.Double, (void*)intPtr, 1);
                    }
                    break;
                default:
                    Console.WriteLine("error: unhandled type in setProperty().");
                    break;
            }
            return this;
        }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern void mpr_obj_push(IntPtr obj);
        public Object push()
        {
            mpr_obj_push(_obj);
            return this;
        }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern IntPtr mpr_obj_get_graph(IntPtr obj);
        public Graph graph()
        {
            return new Graph(mpr_obj_get_graph(_obj));
        }

        internal IntPtr _obj;
    }

    public class Graph : Object
    {
        public enum Event {
            New,            //!< New record has been added to the graph.
            Modified,       //!< The existing record has been modified.
            Removed,        //!< The existing record has been removed.
            Expired         //!< The graph has lost contact with the remote entity.
        }

        public Graph(IntPtr obj) : base(obj) {}

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern IntPtr mpr_graph_new(int flags);
        public Graph(int flags) : base(mpr_graph_new(flags)) {}
        public Graph() : base(mpr_graph_new(0)) {}

        public new Graph setProperty<P, T>(P prop, T value) {
            base.setProperty(prop, value);
            return this;
        }
    }

    public class Signal : Object
    {
        private delegate void HandlerDelegate(IntPtr sig, int evt, UInt64 instance, int length,
                                              int type, IntPtr value, IntPtr time);
        public enum Event {
            NewInstance         = 0x01, //!< New instance has been created.
            UstreamRelease      = 0x02, //!< Instance was released upstream.
            DownstreamRelease   = 0x04, //!< Instance was released downstream.
            Overflow            = 0x08, //!< No local instances left.
            Update              = 0x10, //!< Instance value has been updated.
            All                 = 0x1F
        }

        public enum Stealing {
            None,       //!< No stealing will take place.
            Oldest,     //!< Steal the oldest instance.
            Newest,     //!< Steal the newest instance.
        }

        private enum HandlerType {
            None,
            SingleInt,
            SingleFloat,
            SingleDouble,
            InstanceInt,
            InstanceFloat,
            InstanceDouble
        }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern void mpr_sig_set_cb(IntPtr sig, IntPtr handler, int events);

        // construct from mpr_sig pointer
        internal Signal(IntPtr sig) : base(sig) {}

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        unsafe private static extern void mpr_sig_set_value(IntPtr sig, UInt64 id, int len, int type, void* val);
        unsafe private void _setValue(int value, UInt64 inst) {
            mpr_sig_set_value(this._obj, inst, 1, (int)Type.Int32, (void*)&value);
        }
        unsafe private void _setValue(float value, UInt64 inst) {
            mpr_sig_set_value(this._obj, inst, 1, (int)Type.Float, (void*)&value);
        }
        unsafe private void _setValue(double value, UInt64 inst) {
            mpr_sig_set_value(this._obj, inst, 1, (int)Type.Double, (void*)&value);
        }
        unsafe private void _setValue(int[] value, UInt64 inst) {
            fixed(int* temp = &value[0]) {
                IntPtr intPtr = new IntPtr((void*)temp);
                mpr_sig_set_value(this._obj, inst, value.Length, (int)Type.Int32, (void*)intPtr);
            }
        }
        unsafe private void _setValue(float[] value, UInt64 inst) {
            fixed(float* temp = &value[0]) {
                IntPtr intPtr = new IntPtr((void*)temp);
                mpr_sig_set_value(this._obj, inst, value.Length, (int)Type.Float, (void*)intPtr);
            }
        }
        unsafe private void _setValue(double[] value, UInt64 inst) {
            fixed(double* temp = &value[0]) {
                IntPtr intPtr = new IntPtr((void*)temp);
                mpr_sig_set_value(this._obj, inst, value.Length, (int)Type.Double, (void*)intPtr);
            }
        }
        public Signal setValue<T>(T value) {
            dynamic temp = value;
            _setValue(temp, 0);
            return this;
        }

        public class Instance : Signal
        {
            internal Instance(IntPtr sig, UInt64 inst) : base(sig) {
                this.id = inst;
            }

            public new Instance setValue<T>(T value) {
                dynamic temp = value;
                _setValue(temp, id);
                return this;
            }

            public readonly UInt64 id;
        }

        public Instance instance(UInt64 id) {
            return new Instance(this._obj, id);
        }
        public Instance instance(int id) {
            return new Instance(this._obj, (UInt64)id);
        }

        private void _handler(IntPtr sig, int evt, UInt64 inst, int length,
                              int type, IntPtr value, IntPtr time) {
            switch (this.handlerType) {
                case HandlerType.SingleInt:
                    unsafe {
                        int ivalue = *(int*)value;
                        this.handlers.singleInt(new Signal(sig), (Event)evt, ivalue);
                    }
                    break;
                case HandlerType.SingleFloat:
                    unsafe {
                        float fvalue = *(float*)value;
                        this.handlers.singleFloat(new Signal(sig), (Event)evt, fvalue);
                    }
                    break;
                case HandlerType.SingleDouble:
                    unsafe {
                        double dvalue = *(double*)value;
                        this.handlers.singleDouble(new Signal(sig), (Event)evt, dvalue);
                    }
                    break;
                case HandlerType.InstanceInt:
                    unsafe {
                        int ivalue = *(int*)value;
                        this.handlers.instanceInt(new Signal.Instance(sig, inst), (Event)evt, ivalue);
                    }
                    break;
                case HandlerType.InstanceFloat:
                    unsafe {
                        float fvalue = *(float*)value;
                        this.handlers.instanceFloat(new Signal.Instance(sig, inst), (Event)evt, fvalue);
                    }
                    break;
                case HandlerType.InstanceDouble:
                    unsafe {
                        double dvalue = *(double*)value;
                        this.handlers.instanceDouble(new Signal.Instance(sig, inst), (Event)evt, dvalue);
                    }
                    break;
                default:
                break;
            }
        }

        ~Signal()
            {}

        private Boolean _setCallback(Action<Signal, Signal.Event, int> h, int type)
        {
            if (type != (int)Type.Int32)
                return false;
            handlerType = HandlerType.SingleInt;
            handlers.singleInt = h;
            return true;
        }

        private Boolean _setCallback(Action<Signal, Signal.Event, float> h, int type)
        {
            if (type != (int)Type.Float)
                return false;
            handlerType = HandlerType.SingleFloat;
            handlers.singleFloat = h;
            return true;
        }

        private Boolean _setCallback(Action<Signal, Signal.Event, double> h, int type)
        {
            if (type != (int)Type.Double)
                return false;
            handlerType = HandlerType.SingleDouble;
            handlers.singleDouble = h;
            return true;
        }

        private Boolean _setCallback(Action<Signal.Instance, Signal.Event, int> h, int type)
        {
            if (type != (int)Type.Int32)
                return false;
            handlerType = HandlerType.InstanceInt;
            handlers.instanceInt = h;
            return true;
        }

        private Boolean _setCallback(Action<Signal.Instance, Signal.Event, float> h, int type)
        {
            if (type != (int)Type.Float)
                return false;
            handlerType = HandlerType.InstanceFloat;
            handlers.instanceFloat = h;
            return true;
        }

        private Boolean _setCallback(Action<Signal.Instance, Signal.Event, double> h, int type)
        {
            if (type != (int)Type.Double)
                return false;
            handlerType = HandlerType.InstanceDouble;
            handlers.instanceDouble = h;
            return true;
        }

        // TODO: add vector or array handlers
        // TODO: add instance handlers

        public Signal setCallback<T>(T handler, int events = 0)
        {
            dynamic temp = handler;
            int type = mpr_obj_get_prop_as_int32(this._obj, (int)Property.Type, null);
            if (!_setCallback(temp, type)) {
                Console.WriteLine("error: wrong data type in signal handler.");
                return this;
            }
            mpr_sig_set_cb(this._obj,
                           Marshal.GetFunctionPointerForDelegate(new HandlerDelegate(_handler)),
                           events);
            return this;
        }

        public new Signal setProperty<P, T>(P prop, T value) {
            base.setProperty(prop, value);
            return this;
        }
        public new Signal push() {
            base.push();
            return this;
        }

        [StructLayout(LayoutKind.Explicit)]
        struct Handlers
        {
            [FieldOffset(0)]
            internal Action<Signal, Signal.Event, int> singleInt;
            [FieldOffset(0)]
            internal Action<Signal, Signal.Event, float> singleFloat;
            [FieldOffset(0)]
            internal Action<Signal, Signal.Event, double> singleDouble;
            [FieldOffset(0)]
            internal Action<Signal.Instance, Signal.Event, int> instanceInt;
            [FieldOffset(0)]
            internal Action<Signal.Instance, Signal.Event, float> instanceFloat;
            [FieldOffset(0)]
            internal Action<Signal.Instance, Signal.Event, double> instanceDouble;
        }
        private Handlers handlers;
        private HandlerType handlerType = HandlerType.None;
    }

    public class Device : Object
    {
        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern IntPtr mpr_dev_new([MarshalAs(UnmanagedType.LPStr)] string devname, IntPtr graph);
        public Device(string name)
            : base(mpr_dev_new(name, IntPtr.Zero)) {}

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern IntPtr mpr_dev_free(IntPtr dev);
        ~Device()
            { mpr_dev_free(this._obj); }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern int mpr_dev_get_is_ready(IntPtr dev);
        public int getIsReady()
            { return mpr_dev_get_is_ready(this._obj); }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern int mpr_dev_poll(IntPtr dev, int block_ms);
        public int poll(int block_ms)
            { return mpr_dev_poll(this._obj, block_ms); }
        public int poll()
            { return mpr_dev_poll(this._obj, 0); }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern IntPtr mpr_sig_new(IntPtr dev, int direction,
                                                 [MarshalAs(UnmanagedType.LPStr)] string name,
                                                 int length, int type,
                                                 [MarshalAs(UnmanagedType.LPStr)] string unit,
                                                 IntPtr min, IntPtr max, IntPtr num_inst,
                                                 IntPtr handler, int events);
        public Signal addSignal(Direction dir, string name, int length, Type type,
                                string unit = null, int numInstances = -1)
        {
            IntPtr instPtr = IntPtr.Zero;
            if (numInstances != -1) {
                unsafe {
                    instPtr = new IntPtr(&numInstances);
                }
            }
            IntPtr sigptr = mpr_sig_new(this._obj, (int) dir, name, length, (int) type, unit,
                                        IntPtr.Zero, IntPtr.Zero, instPtr, IntPtr.Zero, 0);
            return new Signal(sigptr);
        }

        public new Device setProperty<P, T>(P prop, T value) {
            base.setProperty(prop, value);
            return this;
        }
        public new Device push() {
            base.push();
            return this;
        }
    }

    public class Map : Object
    {
        public enum Location {
            Source      = 0x01, //!< Source signal(s) for this map.
            Destination = 0x02, //!< Destination signal(s) for this map.
            Any         = 0x03  //!< Either source or destination signals.
        }

        public enum Protocol {
            UDP,              //!< Map updates are sent using UDP.
            TCP               //!< Map updates are sent using TCP.
        }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        unsafe private static extern IntPtr mpr_map_new(int num_srcs, void* srcs, int num_dsts, void* dsts);
        unsafe public Map(Signal src, Signal dst)
        {
            fixed (void* s = &src._obj) {
                fixed (void* d = &dst._obj) {
                    this._obj = mpr_map_new(1, s, 1, d);
                }
            }
        }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern IntPtr mpr_map_new_from_str([MarshalAs(UnmanagedType.LPStr)] string expr,
                                                          IntPtr sig0, IntPtr sig1, IntPtr sig2,
                                                          IntPtr sig3, IntPtr sig4, IntPtr sig5,
                                                          IntPtr sig6, IntPtr sig7, IntPtr sig8,
                                                          IntPtr sig9, IntPtr sig10);
        public Map(string expr, params Signal[] signals)
        {
            IntPtr[] a = new IntPtr[10];
            for (int i = 0; i < 10; i++) {
                if (i < signals.Length)
                    a[i] = signals[i]._obj;
                else
                    a[i] = default(IntPtr);
            }
            this._obj = mpr_map_new_from_str(expr, a[0], a[1], a[2], a[3], a[4], a[5],
                                             a[6], a[7], a[8], a[9], default(IntPtr));
        }

        [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        private static extern int mpr_map_get_is_ready(IntPtr map);
        public int getIsReady()
            { return mpr_map_get_is_ready(this._obj); }

        public new Map setProperty<P, T>(P prop, T value) {
            base.setProperty(prop, value);
            return this;
        }
        public new Map push() {
            base.push();
            return this;
        }
    }

    // [DllImport("mapper", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
    // private static extern IntPtr mpr_get_version();
    // public string getVersion()
    //     { return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(mpr_get_version()); }
}
