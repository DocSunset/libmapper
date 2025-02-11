using System;
using System.Threading;
using Mapper;

public class TestCSharp
{
    private static void handler(Signal.Instance inst, Mapper.Signal.Event evt, float value) {
        Console.WriteLine("insig instance " + inst.id + " received value " + value);
    }

    public static void Main(string[] args) {
        // string version = Mapper.getVersion();
        // Console.WriteLine("mapper version = " + version);

        Device dev = new Device("CSmapper");
        Console.WriteLine("created dev CSmapper");

        Signal outsig = dev.addSignal(Direction.Outgoing, "outsig", 1, Mapper.Type.Float, null, 3);
        Console.WriteLine("created signal outsig");

        Signal insig = dev.addSignal(Direction.Incoming, "insig", 1, Mapper.Type.Float, null, 3)
                          .setCallback((Action<Signal.Instance, Signal.Event, float>)handler,
                                       (int)Mapper.Signal.Event.Update);
        Console.WriteLine("created signal insig");

        Console.Write("Waiting for device");
        while (dev.getIsReady() == 0) {
            dev.poll(25);
        }
        Console.WriteLine("Device ready...");

        // Map map = new Map(outsig, insig);
        Map map = new Map("%y=%x*1000", insig, outsig);
        map.push();

        Console.Write("Waiting for map...");
        while (map.getIsReady() == 0) {
            dev.poll(25);
        }

        float sig_val = 0.0F;
        int counter = 0;
        while (++counter < 100) {
            outsig.instance(counter % 3).setValue(sig_val);
            dev.poll(100);
            sig_val += 0.1F;
            if (sig_val > 100)
                sig_val = 0.0F;
            Console.Write("outsig instance " + counter % 3 + " updated to ");
            Console.WriteLine(sig_val.ToString());
        }
    }
}
