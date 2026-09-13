package com.mgd.odyssey;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.ListAdapter;
import androidx.recyclerview.widget.RecyclerView;

public class StampAdapter extends ListAdapter<Stamp, StampAdapter.StampViewHolder> {

    private final OnStampClickListener listener;

    public interface OnStampClickListener {
        void onStampClick(String kingdom);
    }

    public StampAdapter(OnStampClickListener listener) {
        super(DIFF_CALLBACK);
        this.listener = listener;
    }

    private static final DiffUtil.ItemCallback<Stamp> DIFF_CALLBACK = new DiffUtil.ItemCallback<Stamp>() {
        @Override
        public boolean areItemsTheSame(@NonNull Stamp oldItem, @NonNull Stamp newItem) {
            return oldItem.kingdom.equals(newItem.kingdom);
        }

        @Override
        public boolean areContentsTheSame(@NonNull Stamp oldItem, @NonNull Stamp newItem) {
            return oldItem.equals(newItem);
        }
    };

    @NonNull
    @Override
    public StampViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_stamp, parent, false);
        return new StampViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull StampViewHolder holder, int position) {
        Stamp stamp = getItem(position);
        holder.bind(stamp);
    }

    class StampViewHolder extends RecyclerView.ViewHolder {
        final ImageView art;
        final TextView postmark;
        final TextView title;
        final TextView subtitle;
        final TextView moons;
        final ImageView moonIcon;

        StampViewHolder(@NonNull View itemView) {
            super(itemView);
            art = itemView.findViewById(R.id.stampArt);
            postmark = itemView.findViewById(R.id.stampPostmark);
            title = itemView.findViewById(R.id.stampTitle);
            subtitle = itemView.findViewById(R.id.stampSub);
            moons = itemView.findViewById(R.id.stampMoons);
            moonIcon = itemView.findViewById(R.id.stampMoon);

            itemView.setOnClickListener(v -> {
                int pos = getAdapterPosition();
                if (pos != RecyclerView.NO_POSITION && listener != null) {
                    listener.onStampClick(getItem(pos).kingdom);
                }
            });
        }

        void bind(Stamp stamp) {
            itemView.setActivated(false);
            // Art background based on kingdom
            if (stamp.kingdom.equals("Cap")) {
                itemView.setBackgroundResource(R.drawable.stamp_cap_bg);
            } else if (stamp.kingdom.equals("Cascade")) {
                itemView.setBackgroundResource(R.drawable.stamp_cascade_bg);
            } else if (stamp.kingdom.equals("Sand")) {
                itemView.setBackgroundResource(R.drawable.stamp_sand_bg);
            } else if (stamp.kingdom.equals("Lake")) {
                itemView.setBackgroundResource(R.drawable.stamp_lake_bg);
            }

            // Postmark
            String[] parts = stamp.kingdom.split(" ");
            String code = parts.length > 1 ? parts[0].substring(0, 3).toUpperCase() : stamp.kingdom.substring(0, 3).toUpperCase();
            // postmark text set in XML, could be dynamic

            // Title & subtitle
            ((TextView) itemView.findViewById(R.id.stampTitle)).setText(stamp.displayName);
            ((TextView) itemView.findViewById(R.id.stampSub)).setText(stamp.subtitle);

            // Moons
            ((TextView) itemView.findViewById(R.id.stampMoons)).setText("×" + stamp.moons);
        }
    }
}