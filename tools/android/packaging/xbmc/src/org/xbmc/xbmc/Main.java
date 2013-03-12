package org.xbmc.xbmc;

import android.app.NativeActivity;
import android.content.Intent;
import android.os.Bundle;
import org.xbmc.xbmc.XBMCBroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;

public class Main extends NativeActivity 
{
  public Main() 
  {
    super();
  }

  @Override
  public void onCreate(Bundle savedInstanceState) 
  {
    super.onCreate(savedInstanceState);
    IntentFilter intentFilter = new IntentFilter();
    intentFilter.addDataScheme("file");
    intentFilter.addAction(Intent.ACTION_MEDIA_MOUNTED);
    XBMCBroadcastReceiver broadcastReceiver = new XBMCBroadcastReceiver();
    registerReceiver(broadcastReceiver, intentFilter);
  }
}
