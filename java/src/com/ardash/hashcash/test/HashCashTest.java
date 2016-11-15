package com.ardash.hashcash.test;

import static org.junit.Assert.*;

import java.security.NoSuchAlgorithmException;

import org.junit.Test;

import com.nettgryppa.security.HashCash;

public class HashCashTest {

	@Test
	public void test() throws NoSuchAlgorithmException {
		//HashCash hc = new HashCash("")
		HashCash hc = HashCash.mintCash("foo@bar", 20);
		System.out.println(hc);
//		hc.estimateTime(value)
		fail("Not yet implemented");
	}

}
