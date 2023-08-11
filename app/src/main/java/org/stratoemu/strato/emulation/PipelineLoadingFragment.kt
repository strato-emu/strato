/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.emulation

import android.annotation.SuppressLint
import android.graphics.RenderEffect
import android.graphics.Shader
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.google.android.renderscript.Toolkit
import org.stratoemu.strato.data.BaseAppItem
import org.stratoemu.strato.data.AppItemTag
import org.stratoemu.strato.databinding.PipelineLoadingBinding
import org.stratoemu.strato.utils.serializable

private const val TotalPipelineCountTag = "PipelineLoadingFragment::TotalCount"
private const val PipelineProgressTag = "PipelineLoadingFragment::Progress"

class PipelineLoadingFragment : Fragment() {
    private val item by lazy { requireArguments().serializable<BaseAppItem>(AppItemTag)!! }
    private val totalPipelineCount by lazy { requireArguments().getInt(TotalPipelineCountTag) }

    private lateinit var binding : PipelineLoadingBinding

    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) = PipelineLoadingBinding.inflate(inflater).also { binding = it }.root

    @SuppressLint("SetTextI18n")
    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.gameTitle.apply {
            text = item.title
            isSelected = true
        }
        binding.gameVersion.text = item.version
        binding.gameIcon.setImageBitmap(item.bitmapIcon)

        val progress = savedInstanceState?.getInt(PipelineProgressTag) ?: 0
        binding.progressBar.max = totalPipelineCount
        updateProgress(progress)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            binding.backgroundImage.setImageBitmap(item.bitmapIcon)
            binding.backgroundImage.setRenderEffect(RenderEffect.createBlurEffect(75f, 75f, Shader.TileMode.MIRROR))
        } else {
            binding.backgroundImage.setImageBitmap(Toolkit.blur(item.bitmapIcon, 15))
        }
    }

    override fun onSaveInstanceState(outState : Bundle) {
        super.onSaveInstanceState(outState)
        outState.putInt(PipelineProgressTag, binding.progressBar.progress)
    }

    @SuppressLint("SetTextI18n")
    fun updateProgress(progress : Int) {
        if (!this::binding.isInitialized)
            return

        binding.progressBar.progress = progress
        binding.progressLabel.text = "$progress/$totalPipelineCount (${(progress.toFloat() / totalPipelineCount * 100).toInt()}%)"
    }

    companion object {
        fun newInstance(item : BaseAppItem, totalPipelineCount : Int) = PipelineLoadingFragment().apply {
            arguments = Bundle().apply {
                putSerializable(AppItemTag, item)
                putInt(TotalPipelineCountTag, totalPipelineCount)
            }
        }
    }
}
