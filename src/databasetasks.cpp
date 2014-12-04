/**
 * The Forgotten Server - a free and open-source MMORPG server emulator
 * Copyright (C) 2014  Mark Samman <mark.samman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "otpch.h"

#include "databasetasks.h"
#include "database.h"
#include "tasks.h"

extern Dispatcher g_dispatcher;

DatabaseTasks::DatabaseTasks()
{
	threadState = THREAD_STATE_TERMINATED;
}

void DatabaseTasks::start()
{
	threadState = THREAD_STATE_RUNNING;
	thread = std::thread(&DatabaseTasks::run, this);
}

void DatabaseTasks::run()
{
	std::unique_lock<std::mutex> taskLockUnique(taskLock, std::defer_lock);
	while (threadState != THREAD_STATE_TERMINATED) {
		taskLockUnique.lock();
		if (tasks.empty()) {
			taskSignal.wait(taskLockUnique);
		}

		if (!tasks.empty() && threadState != THREAD_STATE_TERMINATED) {
			const std::string& query = tasks.front();
			taskLockUnique.unlock();
			db.executeQuery(query);
			taskLockUnique.lock();
			tasks.pop_front();
		}
		taskLockUnique.unlock();
	}
}

void DatabaseTasks::addTask(const std::string& query)
{
	bool signal = false;
	taskLock.lock();
	if (threadState == THREAD_STATE_RUNNING) {
		signal = tasks.empty();
		tasks.emplace_back(query);
	}
	taskLock.unlock();

	if (signal) {
		taskSignal.notify_one();
	}
}

void DatabaseTasks::flush()
{
	while (!tasks.empty()) {
		db.executeQuery(tasks.front());
		tasks.pop_front();
	}
}

void DatabaseTasks::stop()
{
	taskLock.lock();
	threadState = THREAD_STATE_CLOSING;
	taskLock.unlock();
}

void DatabaseTasks::shutdown()
{
	taskLock.lock();
	threadState = THREAD_STATE_TERMINATED;
	flush();
	taskLock.unlock();
	taskSignal.notify_one();
}

void DatabaseTasks::join()
{
	thread.join();
}
