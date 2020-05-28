/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import android.content.Context
import android.util.Log
import java.io.*

/**
 * This class is used to manage all transactions with storing/retrieving data in relation to input
 */
class InputManager constructor(val context : Context) {
    /**
     * The underlying [File] object with the input data
     */
    private val file = File("${context.applicationInfo.dataDir}/input.bin")

    /**
     * A [HashMap] of all the controllers that contains their metadata
     */
    lateinit var controllers : HashMap<Int, Controller>

    /**
     * A [HashMap] between all [HostEvent]s and their corresponding [GuestEvent]s
     */
    lateinit var eventMap : HashMap<HostEvent?, GuestEvent?>

    init {
        var readFile = false

        try {
            if (file.exists() && file.length() != 0L) {
                syncObjects()
                readFile = true
            }
        } catch (e : Exception) {
            Log.e(this.toString(), e.localizedMessage ?: "InputManager cannot read \"${file.absolutePath}\"")
        }

        if (!readFile) {
            controllers = hashMapOf(
                    0 to Controller(0, ControllerType.None),
                    1 to Controller(1, ControllerType.None),
                    2 to Controller(2, ControllerType.None),
                    3 to Controller(3, ControllerType.None),
                    4 to Controller(4, ControllerType.None),
                    5 to Controller(5, ControllerType.None),
                    6 to Controller(6, ControllerType.None),
                    7 to Controller(7, ControllerType.None))

            eventMap = hashMapOf()

            syncFile()
        }
    }

    /**
     * This function syncs the class with data from [file]
     */
    fun syncObjects() {
        val fileInput = FileInputStream(file)
        val objectInput = ObjectInputStream(fileInput)

        @Suppress("UNCHECKED_CAST")
        controllers = objectInput.readObject() as HashMap<Int, Controller>

        @Suppress("UNCHECKED_CAST")
        eventMap = objectInput.readObject() as HashMap<HostEvent?, GuestEvent?>
    }

    /**
     * This function syncs [file] with data from the class and eliminates unused value from the map
     */
    fun syncFile() {
        val fileOutput = FileOutputStream(file)
        val objectOutput = ObjectOutputStream(fileOutput)

        for (controller in controllers.values) {
            for (button in ButtonId.values()) {
                if (button != ButtonId.Menu && !(controller.type.buttons.contains(button) || controller.type.sticks.any { it.button == button })) {
                    val guestEvent = ButtonGuestEvent(controller.id, button)

                    eventMap.filterValues { it is ButtonGuestEvent && it == guestEvent }.keys.forEach { eventMap.remove(it) }
                }
            }

            for (stick in StickId.values()) {
                if (!controller.type.sticks.contains(stick)) {
                    for (axis in arrayOf(stick.xAxis, stick.yAxis)) {
                        for (polarity in booleanArrayOf(true, false)) {
                            val guestEvent = AxisGuestEvent(controller.id, axis, polarity)

                            eventMap.filterValues { it is AxisGuestEvent && it == guestEvent }.keys.forEach { eventMap.remove(it) }
                        }
                    }
                }
            }
        }

        objectOutput.writeObject(controllers)
        objectOutput.writeObject(eventMap)

        objectOutput.flush()
    }
}
