package com.ardash.hashcash.test;
import static org.junit.Assert.assertTrue;

import java.security.NoSuchAlgorithmException;

import junit.framework.Assert;

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
		// just make sure they run without error for now
		for (int i = 0; i <= HashCash.HASH_LENGTH; i++) {
			long t = HashCash.estimateTime(i);
			assertTrue(t>=0);
		}
	}

}
