package com.ardash.hashcash.test;

/*
Copyright 2016 Andreas Redmer <a_r@posteo.de>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.BeforeClass;
import org.junit.Test;

import com.ardash.hashcash.HashCash;

public class EstimationsTest {
	
	@BeforeClass
	public static void initTests()
	{
		HashCash.estimateTime(1); // init the estimations
	}

	@Test
	public void testEstimates(){
		long tMax =0;
		// just make sure they run without error for now
		for (int i = 0; i <= HashCash.HASH_LENGTH; i++) {
			long t = HashCash.estimateTime(i);
			tMax = Math.max(tMax, t);
			assertTrue(t>=0);
		}
		
		// reverse check: estimation for time_24
		long millisFor24 = HashCash.estimateTime(24);
		int secFor24 = (int)millisFor24/1000;
		int valFor24 = HashCash.estimateValue(secFor24+1);
		assertEquals(24, valFor24);
		
		// check if half of the time reduced value by 1 bit only
		//assertEquals(HashCash.HASH_LENGTH-1, valForTMax/2);
	}

	@Test
	public void testcompareEstimatesToRealityTest() {
		final int MIN_TEST_PER_VAL = 3;
		final long MAX_DELTA = 10000;
		for (int valToTest = 14; valToTest<=24;valToTest++)
		{
			long estimatedMillisecondsToRun = 1000; // for me to reduce the time of the test
			long millisEstimateForOneMint = HashCash.estimateTime(valToTest);
			long amountOfTestsToRun = estimatedMillisecondsToRun/millisEstimateForOneMint;
			amountOfTestsToRun = Math.max(MIN_TEST_PER_VAL, amountOfTestsToRun); // don't run 0 tests - at least 10
			long estimationForAllTestsWithThisValue = millisEstimateForOneMint*amountOfTestsToRun;
			
			HashCash.mintCash("test@nowhere", valToTest); // one for the optimizer

			long actualStartTime = System.currentTimeMillis();
			for (long i=0; i<amountOfTestsToRun; i++)
			{
				HashCash.mintCash("test@nowhere", valToTest);
			}
			long actualRunTime = System.currentTimeMillis()-actualStartTime;
			long actualRunTimePerMint = actualRunTime/amountOfTestsToRun;
			
			// removed abs() time, so too low estimation is ignored
			long delta = actualRunTimePerMint-millisEstimateForOneMint;
			// check if estimation was less than 10 second wrong
			assertTrue("Estimation for value "+valToTest+" was "+millisEstimateForOneMint+" but real runtime was "+actualRunTimePerMint,delta<MAX_DELTA);
		}

	}
}
