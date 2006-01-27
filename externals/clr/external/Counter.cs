using System;

/// <summary>
/// Descrizione di riepilogo per Counter.
/// </summary>
public class Counter:
	PureData.External
{
	public Counter()
	{
		Post("Count");

//        EventFloat += new MethodFloat(MyFloat);
    }

	public Counter(PureData.Atom[] args)
	{
        Post("Count with args");

//        pd.AddInlet(x, "init", ParametersType.Float);
//        pd.AddOutlet(x, ParametersType.Float);
    }

	// this function MUST exist
	public static void Main()
	{
        Post("Count.Main");       
	}

/*
    public void MyBang() 
    { 
        Post("Count-BANG"); 
    }

    public void MyFloat(float f)
    {
        Post(String.Format("Count-FLOAT {0}",f));       
    }
*/    
    protected override void MethodBang() 
    { 
        Post("Count-BANG"); 
    }

    protected override void MethodFloat(float f) 
    { 
        Post("Count-FLOAT "+f.ToString()); 
    }

    protected override void MethodSymbol(PureData.Symbol s) 
    { 
        Post("Count-SYMBOL "+s.ToString()); 
    }

    /*
	public void Init(float f)
	{
		curr  = (int) f;
	}

	public void SendOut()
	{
		pd.SendToOutlet(x, 0, new Atom(curr));
	}

	public void Sum(float f)
	{
		curr += (int) f;
		pd.SendToOutlet(x, 0, new Atom(curr));
	}

*/
}
