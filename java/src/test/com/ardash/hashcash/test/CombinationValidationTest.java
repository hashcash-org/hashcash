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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Collection;
import java.util.GregorianCalendar;
import java.util.Iterator;
import java.util.List;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import com.ardash.hashcash.HashCash;

@RunWith(Parameterized.class)
public class CombinationValidationTest {
    @Parameters
    public static Collection<Object[]> data() {
        final String charsToTest = "'\"\\|;.,<>/?!@#$%^&*()_-=+[]{}()a1`~ "; // no colon
        
        Calendar startDate = new GregorianCalendar(1000,1,1);
        int daysIn123Years = 365*123;
      
        // build dates
        ArrayList<Calendar> dates = new ArrayList<Calendar>(9);
        for (int i = 1; i<=10; i++)
        {
        	dates.add(startDate);
        	startDate.add(Calendar.DAY_OF_MONTH, daysIn123Years);
        }
        
        // build claimed
        ArrayList<Integer> claimedBits = new ArrayList<Integer>(30);
        claimedBits.add(2);
        claimedBits.add(3);

        // build resources and extensions
        ArrayList<String> resources = new ArrayList<String>();
        ArrayList<String> extensions = new ArrayList<String>();
        for (int i=0; i<charsToTest.length()-1;i++)
        {
        	final char ch1 = charsToTest.charAt(i);
        	final char ch2 = charsToTest.charAt(i+1);
			resources.add(ch1+"");
        	extensions.add(ch1+"");
			resources.add(ch1+""+ch2);
        }

        // 34*34*20*10=231200
        // print runtime estimation
        int allCombos=resources.size()*extensions.size()*dates.size()*claimedBits.size();
        System.out.println("allCombos: " + allCombos);
        int minted = 0; // counter
        List<Object[]> combinations = new ArrayList<Object[]>(allCombos);
        
        for (String resource : resources) {
            for (String extension : extensions) {
                for (Calendar date : dates) {
                    for (int demandedValue : claimedBits) {
                        HashCash stamp = HashCash.mintCash(resource, extension, date, demandedValue, 1);
                        combinations.add(new Object[] {stamp.toString(),demandedValue,true});
                        combinations.add(new Object[] {stamp.toString(),demandedValue-1,true});
                        combinations.add(new Object[] {stamp.toString(),demandedValue+1,false});
                        minted ++;
            		}
        		}
    		}
            // print status
            System.out.println("minted: "+minted+ " of "+allCombos);
		}
        
        List<Object[]> ret = Arrays.asList(new Object[][] {
        { "1:48:110416:etienne@cri.fr:::000A2F00000063BF012", 48, true },
        { "1:44:070217:foo::xSi0bPjoswUh6h1Y:TMNI7", 44, true },
        { "1:42:060922:When I think of all the good times that I've wasted ...::UXkz/DsCCgfvBVtH:00000EF7+j", 42, true },
        { "1:41:060704:president@whitehouse.gov::XxcHzSfxDZ38cwRu:000000000000000000000000000000000000m2EDd", 41, true },
        { "1:40:051222:foo@bar.org::Cu2iqc4SmotZ7MRR:0000214c3J", 40, true },
        { "1:20:040806:foo::65f460d0726f420d:13a6b8", 20, true },

        // TODO add Adam Back's tests
        
        { "1:1:111111::::", 0, false },
        { "1:0:111111::::", 0, true }
           });
        
        combinations.addAll(ret);
		return combinations;
    }

    private String inputStamp;
    private int bitsExpected;
    private boolean validExpected;

    public CombinationValidationTest(String inputStamp, int bitsExpected, boolean validExpected) {
    	this.inputStamp= inputStamp;
        this.bitsExpected= bitsExpected;
        this.validExpected= validExpected;
    }

    @Test
    public void test() {
    	HashCash hc = HashCash.parseFromString(inputStamp);
        assertTrue(hc.isValid()==validExpected);
        assertEquals(hc.getValue(), bitsExpected);
    }
}