package com.github.jtcressy.magspoofbt;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.design.card.MaterialCardView;
import android.support.v4.app.Fragment;
import android.support.v7.widget.CardView;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;


public class DeviceSettingsFragment extends Fragment {

    public View.OnClickListener scanButtonListener;
    public View.OnClickListener unpairButtonListener;
    private ArrayList<DeviceScanResult> deviceScans;


    private RecyclerView mRecyclerView;
    private RecyclerView.Adapter mAdapter;
    private RecyclerView.LayoutManager mLayoutManager;

    public class DeviceScanResult{
        public String deviceName;
        public String serialNumber;
        public View.OnClickListener pairButtonListener;
        private void pairCallback() {
            Toast.makeText(getContext(), "pairCallback was called for " + deviceName, Toast.LENGTH_SHORT).show();
        }

        public DeviceScanResult(String deviceName, String serialNumber) {
            this.deviceName = deviceName;
            this.serialNumber = serialNumber;
            pairButtonListener = new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    pairCallback();
                }
            };
        }
    }


    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        View v = inflater.inflate(R.layout.fragment_device_settings, container, false);

        // Button Stuff
        scanButtonListener = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                deviceScans.add(new DeviceScanResult("MagSpoofBT"+deviceScans.size(), "MGSPF0000-000"+deviceScans.size()));
                mAdapter.notifyDataSetChanged();
            }
        };
        unpairButtonListener = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Toast.makeText(getContext(), "unpair button clicked", Toast.LENGTH_SHORT).show();
            }
        };
        Button scanButton = v.findViewById(R.id.scanButton);
        Button unpairButton = v.findViewById(R.id.unpairButton);
        scanButton.setOnClickListener(scanButtonListener);
        unpairButton.setOnClickListener(unpairButtonListener);
        // End Button Stuff

        deviceScans = new ArrayList<DeviceScanResult>();

        // Scan results view setup
        mRecyclerView = v.findViewById(R.id.deviceListView);
        mRecyclerView.setHasFixedSize(true);
        mLayoutManager = new LinearLayoutManager(getActivity());
        mRecyclerView.setLayoutManager(mLayoutManager);
        mAdapter = new DeviceScanAdapter(deviceScans);
        mRecyclerView.setAdapter(mAdapter);
        // End Scan results view setup


        return v;
    }


    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }
}


