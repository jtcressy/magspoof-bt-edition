package com.github.jtcressy.magspoofbt;

import android.support.design.card.MaterialCardView;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import java.util.ArrayList;


public class DeviceScanAdapter extends RecyclerView.Adapter<DeviceScanAdapter.ViewHolder> {
    private ArrayList<DeviceSettingsFragment.DeviceScanResult> mDataset;


    public static class ViewHolder extends RecyclerView.ViewHolder {
        private TextView serialNumber, deviceName;
        private Button pairButton;
        public ViewHolder(MaterialCardView v) {
            super(v);
            deviceName = v.findViewById(R.id.deviceName);
            serialNumber = v.findViewById(R.id.serialNumber);
            pairButton = v.findViewById(R.id.scanButton);
        }
    }


    public DeviceScanAdapter(ArrayList<DeviceSettingsFragment.DeviceScanResult> dataset) {
        mDataset = dataset;
    }


    public DeviceScanAdapter.ViewHolder onCreateViewHolder(ViewGroup parent,
                                                           int viewType) {
        MaterialCardView v = (MaterialCardView) LayoutInflater.from(parent.getContext())
                .inflate(R.layout.device_description_card, parent, false);
        //TODO: Set view size, margins, paddings, etc.
        ViewHolder  vh = new ViewHolder(v);
        return vh;
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        holder.deviceName.setText(mDataset.get(position).deviceName);
        holder.serialNumber.setText(mDataset.get(position).serialNumber);
        holder.pairButton.setOnClickListener(mDataset.get(position).pairButtonListener);
    }

    @Override
    public int getItemCount() {
        return mDataset.size();
    }
}